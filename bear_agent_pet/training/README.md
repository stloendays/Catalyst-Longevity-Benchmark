# Tony English Persona Corpus

This directory contains the English persona corpus for Tony, the C++20 + Qt 6 desktop companion.

## Current v2 dataset

The v2 compiler combines two layers:

- 48 hand-curated seed conversations at weight `1.0`.
- 200 deterministic persona expansion conversations at weight `0.8`.

This produces up to 248 unique examples before deterministic duplicate removal. The corpus intentionally repeats *character traits* across varied situations without repeating the same prompt wording.

The dataset emphasizes Tony's teddy-like cuteness, sensitivity to cold, Chinese background, chemistry studies, soft crush on Paula, handsome no-glasses look, love of hugs, polite requests for cuddles, desktop behavior, and technically reliable Agent behavior.

## Files

- `TONY_PERSONA_EN.md` — canonical character specification and behavior constraints.
- `source/tony_persona_en_v1.json` — 48 high-weight hand-curated conversations.
- `scripts/persona_expansion.py` — 50 scenario families x 4 distinct phrasings = 200 expansion conversations.
- `scripts/compile_persona_corpus.py` — merges, validates, de-duplicates and creates deterministic chat-SFT train/eval files.
- `processed/tony_persona_en_v2/` — generated `all.jsonl`, `train.jsonl`, `eval.jsonl`, and `manifest.json`.

## Training record

Each compiled row contains standard chat messages plus metadata:

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

## Compile

From the repository root:

```bash
python3 bear_agent_pet/training/scripts/compile_persona_corpus.py
```

The default output is `bear_agent_pet/training/processed/tony_persona_en_v2/`. The holdout split uses a deterministic SHA-256 bucket, so evaluation examples remain stable across machines and reruns.

The GitHub Action `Validate Tony persona corpus` checks minimum corpus size, train/eval split, unique prompts, required persona tags, required action coverage, and row schema before publishing the generated dataset as an artifact.

## Fine-tuning policy

This remains a persona dataset, not the complete training mixture. A later Qwen LoRA/QLoRA run should mix it with chemistry reasoning, server/OpenClaw tool traces, desktop action-selection traces, ordinary conversations, refusal/boundary cases, and a frozen evaluation suite. Persona examples should not overwhelm technical examples; Tony must remain scientifically precise even when his presentation is cute.

Do not train on secrets, API keys, private server configuration, or unreviewed personal data.
