# Tony Desktop Pet — Windows Quick Start

Tony is a C++20 + Qt 6 desktop companion backed by the OpenClaw Agent on the research server.

## Run

Unzip the GitHub Actions artifact and start `TonyDesktopPet.exe`. Keep the `assets` directory beside the executable.

Tony is frameless, transparent and always on top. Drag him with the left mouse button, double-click to ask a question, or right-click for local actions such as hug, walk, study chemistry, adjust glasses, no-glasses mode, celebrate, shiver and sleep.

## Server connection

The server Gateway deliberately listens only on `127.0.0.1:18790`; it is not exposed as an unauthenticated public port.

By default Tony starts the Windows `ssh` executable and requests this local forward:

```text
localhost:18790 -> SSH -> ubuntu@150.158.27.206 -> 127.0.0.1:18790
```

Tony does **not** contain or download a private SSH key. Windows must already be able to authenticate to the server with its normal OpenSSH configuration, SSH agent, or a key path stored in Tony's local settings. If the tunnel is not ready immediately, Tony's WebSocket client retries automatically.

This SSH transport is the secure development transport. A later user-facing release should use authenticated WSS/pairing so a friend does not need server SSH credentials.

## Current behavior

Temporary character actions take precedence over long-running Agent states. For example, a cold-related prompt can make Tony shiver briefly; after that animation he returns to the underlying `thinking` or `working` state until the Agent finishes.

The English persona corpus and animation action labels live under `../training/` and are kept separate from private credentials and server configuration.
