# Tony operator-log archive

Tony records operator activity locally in JSONL so the data can later support debugging, usage analysis, and opt-in fine-tuning preparation.

## Privacy model

This repository is public, so raw operator text must never be committed here.

- The Windows client keeps full JSONL locally under Tony's application-data log directory.
- Likely credentials such as API keys, GitHub tokens, passwords, secrets, and JWTs are redacted before the local event is written.
- When Tony detects/downloads an update, pending JSONL is sent through the existing authenticated WSS channel to the Tony gateway.
- The gateway encrypts each batch with a Fernet key stored only on the server at `~/.local/share/bear-agent/operator-log.key`.
- GitHub archives only `*.jsonl.enc` ciphertext plus non-sensitive `*.meta.json` metadata under `operator_logs/batches/`.
- A server acknowledgement moves the client's successfully uploaded batch from `outbox/` to the local `archive/`; local history is retained.

Do not commit the Fernet key or decrypted JSONL to this repository.

## Event schema

A local plaintext event uses JSONL with fields such as:

```json
{"schema":1,"event_id":"...","ts_utc":"...","session_id":"...","event":"chat_submit","app_version":"0.9.0","input":"hello Tony","details":{"language":"en"}}
```

Pet interactions are stored as event names (for example `pet_click`, `pet_drag`, `pet_double_click`, `pet_context_menu`) without screen coordinates. Pairing codes, bearer tokens, and server credentials are not recorded.
