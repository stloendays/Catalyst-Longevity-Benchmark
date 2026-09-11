# Tony English Persona Corpus

This directory contains the English seed corpus for Tony, the C++20 + Qt 6 desktop companion.

## Files

- `TONY_PERSONA_EN.md` — canonical character specification and behavior constraints.
- `raw/tony_persona_en_v1.jsonl` — 90 curated English SFT examples covering identity, teddy-like cuteness, cold sensitivity, China background, chemistry study, Paula, glasses/handsome behavior, hugs, desktop actions, and serious Agent behavior.

## Format

Each JSONL row contains a `messages` array compatible with chat-style supervised fine-tuning, plus metadata for filtering and later desktop action alignment.

Example:

```json
{
  "messages": [
    {"role": "system", "content": "..."},
    {"role": "user", "content": "Tony, what do you want?"},
    {"role": "assistant", "content": "Can I have a hug? Just one. A proper warm one. *holds out both paws*"}
  ],
  "tags": ["hug", "cute"],
  "action": "ask_hug",
  "emotion": "needy",
  "source": "synthetic_tony_persona_en_v1",
  "weight": 1.0
}
```

## Recommended use

Treat this as a persona seed, not a full standalone fine-tuning dataset. Later corpora should add:
1. chemistry reasoning and explanation examples,
2. tool-use / server-Agent traces,
3. desktop action-selection examples,
4. ordinary everyday conversations,
5. refusal / consent / boundary examples,
6. evaluation sets held out from training.

Do not train on secrets, API keys, private server configuration, or unreviewed personal data.
