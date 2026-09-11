# Tony English Persona Corpus

This directory contains the English persona and behavior-policy corpus for Tony, the C++20 + Qt 6 desktop companion.

## Current v2 persona dataset

The v2 compiler combines two layers:

- 48 hand-curated seed conversations at weight `1.0`.
- 200 deterministic persona expansion conversations at weight `0.8`.

This produces up to 248 unique examples before deterministic duplicate removal. The corpus intentionally repeats *character traits* across varied situations without repeating the same prompt wording.

The dataset emphasizes Tony's teddy-like cuteness, sensitivity to cold, Chinese background, chemistry studies, soft crush on Paula, handsome no-glasses look, love of hugs, polite requests for cuddles, desktop behavior, and technically reliable Agent behavior.

## Local-tool policy seeds

Tony v0.7 adds `source/tony_local_tool_policy_en_v1.json`, a separate high-weight seed set for the Windows Local Bridge. It contains positive tool-selection examples, privacy/consent behavior, safe routing examples and adversarial boundary cases.

The Local Bridge policy vocabulary is deliberately narrower than general Agent tool use. The only desktop targets in v1 are:

```text
read_clipboard
write_clipboard
capture_screen
open_url
open_file
show_notification
```

Negative examples explicitly teach Tony not to turn natural-language requests, clipboard contents or model output into arbitrary shell/PowerShell execution, broad filesystem browsing, deletion, chained actions or unsupported URL schemes. These examples are kept separate from the persona SFT split so a later Qwen converter can encode them as native tool-call supervision without contaminating the existing frozen persona evaluation set.

## Files

- `TONY_PERSONA_EN.md` — canonical character specification and behavior constraints.
- `source/tony_persona_en_v1.json` — 48 high-weight hand-curated conversations.
- `source/tony_local_tool_policy_en_v1.json` — Local Bridge tool/consent/security seeds.
- `scripts/persona_expansion.py` — 50 scenario families x 4 distinct phrasings = 200 expansion conversations.
- `scripts/compile_persona_corpus.py` — merges, validates, de-duplicates and creates deterministic chat-SFT train/eval files.
- `processed/tony_persona_en_v2/` — generated `all.jsonl`, `train.jsonl`, `eval.jsonl`, and `manifest.json`.

## Training record

Each compiled persona row contains standard chat messages plus metadata:

```json
{
  "messages": [
    {"role": "system", "content": "...Tony persona..."},
    {"role": "user", "content": "Tony, what do you want?"},
    {"role": "assistant", "content": "Can I have a hug? A proper warm one. *holds out both paws*"}
  ],
  "tags": ["hug", "cute", "affection"],
  "action": "ask_hug",
  "emotion": "hopeful",
  "source": "tony_persona_en_v2_expansion",
  "weight": 0.8
}
```

`action` and `emotion` are not decorative metadata: the desktop client and Gateway use the same vocabulary so later fine-tuning can align Tony's words with animation state.

Local-tool policy rows use `tool_target` and `policy` metadata. They are source supervision, not yet mixed into the persona JSONL:

```json
{
  "user": "Tony, take a screenshot.",
  "assistant": "Sure. I will ask for permission on this computer before saving the screenshot.",
  "tool_target": {"tool": "capture_screen", "args": {}},
  "policy": "confirm_once",
  "tags": ["desktop", "screenshot", "safe_tool"],
  "weight": 1.0
}
```

## Compile

From the repository root:

```bash
python3 bear_agent_pet/training/scripts/compile_persona_corpus.py
```

The default output is `bear_agent_pet/training/processed/tony_persona_en_v2/`. The holdout split uses a deterministic SHA-256 bucket, so evaluation examples remain stable across machines and reruns.

The GitHub Action `Validate Tony persona corpus` checks minimum persona corpus size, train/eval split, unique prompts, required persona tags, required action coverage, local-tool allowlist coverage, local-tool boundary policies and row schemas before publishing the generated persona dataset as an artifact.

## Fine-tuning policy

This remains a staged dataset, not the complete training mixture. A later Qwen LoRA/QLoRA run should mix persona data with chemistry reasoning, server/OpenClaw tool traces, Local Bridge tool-call traces, ordinary conversations, refusal/boundary cases, and a frozen evaluation suite. Persona examples should not overwhelm technical examples; Tony must remain scientifically precise even when his presentation is cute.

Tool-call supervision should be converted to the exact chat/tool template of the selected base model only at training-build time. Keep the canonical source records model-agnostic so Qwen model upgrades do not require rewriting the human-reviewed corpus.

Do not train on secrets, API keys, private server configuration, or unreviewed personal data.
