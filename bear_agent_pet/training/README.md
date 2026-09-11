# Tony English Persona Corpus

This directory contains the English seed corpus for Tony, the C++20 + Qt 6 desktop companion.

## Files

- `TONY_PERSONA_EN.md` — canonical character specification and behavior constraints.
- `source/tony_persona_en_v1.json` — editable source corpus with 48 curated English conversations.
- `scripts/compile_persona_corpus.py` — validates the source corpus and builds deterministic chat-SFT JSONL train/eval files.
- `processed/tony_persona_en_v1/` — generated training files; this directory is produced by the compiler and does not need to be hand-edited.

The seed set emphasizes Tony's teddy-like cuteness, sensitivity to cold, Chinese background, chemistry studies, soft crush on Paula, handsome no-glasses look, love of hugs, polite requests for cuddles, and desktop-pet actions.

## Source format

The editable source keeps the system persona once and stores compact examples:

```json
{
  "user": "Tony, what do you want?",
  "assistant": "Can I have a hug? Just one. A proper warm one. *holds out both paws*",
  "tags": ["hug", "cute"],
  "action": "ask_hug",
  "emotion": "needy"
}
```

The compiler expands each example into a standard chat-SFT record with `system`, `user`, and `assistant` messages. It preserves `tags`, `action`, and `emotion` as metadata so the same corpus can later support desktop animation alignment.

## Compile

From the repository root:

```bash
python3 bear_agent_pet/training/scripts/compile_persona_corpus.py
```

This produces `all.jsonl`, `train.jsonl`, `eval.jsonl`, and `manifest.json`. The holdout split is deterministic, so evaluation examples do not jump between runs.

## Recommended use

Treat this as a persona seed, not a full standalone fine-tuning dataset. Later corpora should add chemistry reasoning, tool-use/server-Agent traces, action-selection examples, ordinary conversations, and a larger frozen evaluation set. Technical reliability should remain stronger than persona styling: Tony may be cute while still being precise about chemistry, code, server state, uncertainty, and evidence.

Do not train on secrets, API keys, private server configuration, or unreviewed personal data.
