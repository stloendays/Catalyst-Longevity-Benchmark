# Tony Desktop Pet — Windows Quick Start

Tony is a C++20 + Qt 6 desktop companion backed by the Tony Gateway, local Qwen3.5-2B and the full OpenClaw Agent on the research server.

## Run

Unzip the GitHub Actions artifact and start `TonyDesktopPet.exe`. Keep the `assets` directory beside the executable.

Tony is frameless, transparent and always on top. Drag him with the left mouse button, double-click to ask a question, or right-click for actions such as hug, walk, study chemistry, adjust glasses, no-glasses mode, celebrate, shiver and sleep.

## Connection modes

### 1. Owner / development: SSH tunnel

By default Tony requests this Windows OpenSSH forward:

```text
localhost:18790 -> SSH -> ubuntu@150.158.27.206 -> 127.0.0.1:18790
```

Tony never embeds the server SSH private key. The Windows account must already be able to authenticate to the server.

### 2. Friend / normal user: WSS pairing

Tony v0.4 also supports per-device pairing. Right-click Tony and choose `连接 / 配对新设备…`, then enter the WSS endpoint and the one-time code issued by the server administrator.

The pairing endpoint returns a random per-device bearer token. On Windows, Tony protects that token with DPAPI before storing it in local settings. The server stores only the token hash, so the plaintext device token is not retained server-side.

A server administrator can issue a one-time code with:

```bash
cd /home/ubuntu/.local/share/bear-agent/app
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli issue --ttl 600
```

The code expires and can be consumed only once. Do not post pairing codes in public GitHub Actions logs.

## Multi-state artwork

Tony now supports separate PNG artwork under `assets/states/` for idle, working, walk, thinking, celebrate, sleep, shiver, ask-hug, hug, blush, study, glasses, no-glasses and wave states. Missing state files automatically fall back to `assets/tony.png`, so artwork can be added incrementally.

Temporary character actions take precedence over long-running Agent states. For example, a cold-related prompt can make Tony shiver briefly; after that animation he returns to the underlying `thinking` or `working` state until the Agent finishes.
