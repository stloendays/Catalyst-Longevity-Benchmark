# Tony Desktop Pet v0.6.1 — Windows Quick Start

Tony is a C++20 + Qt 6 desktop companion backed by the Tony Gateway, local Qwen3.5-2B and the full OpenClaw Agent on the research server.

## Run

Unzip the GitHub Actions artifact and start `TonyDesktopPet.exe`. Keep the `assets` directory beside the executable. The runtime loads both static state images and optional multi-frame animation folders from that directory.

Tony is frameless, transparent and always on top. Drag him with the left mouse button, double-click to ask a question, or right-click for actions such as hug, walk, study chemistry, adjust glasses, no-glasses mode, Paula greeting, celebrate, shiver and sleep.

## Compact chat composer

Double-clicking Tony now opens Tony's own compact floating input card instead of a standard Windows input dialog. The card follows Tony if he is moved, sends on Enter, and closes after submission. Press Escape to dismiss it.

Agent replies continue to stream into Tony's separate speech bubble. This keeps the interaction model visually consistent:

```text
Double-click Tony
  -> compact input card
  -> Enter / Send
  -> thinking animation
  -> working animation when tools run
  -> streaming speech bubble
  -> celebrate / contextual persona action
```

## Connection modes

### 1. Owner / development: SSH tunnel

By default Tony requests this Windows OpenSSH forward:

```text
localhost:18790 -> SSH -> ubuntu@150.158.27.206 -> 127.0.0.1:18790
```

Tony never embeds the server SSH private key. The Windows account must already be able to authenticate to the server.

### 2. Friend / normal user: WSS pairing

Right-click Tony and choose `连接 / 配对新设备…`, then enter the WSS endpoint and the one-time code issued by the server administrator.

The pairing endpoint returns a random per-device bearer token. On Windows, Tony protects that token with DPAPI before storing it in local settings. The server stores only the token hash, so the plaintext device token is not retained server-side.

A server administrator can issue a one-time code with:

```bash
cd /home/ubuntu/.local/share/bear-agent/app
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli issue --ttl 600
```

The code expires and can be consumed only once. Do not post pairing codes in public GitHub Actions logs.

## v0.6.1 artwork and frame playback

Static state images live under `assets/states/`:

```text
idle.png
thinking.png
working.png
walk.png
celebrate.png
sleep.png
shiver.png
ask_hug.png
hug.png
blush.png
blush_wave.png
study.png
adjust_glasses.png
remove_glasses.png
wave.png
```

Every canonical state can now optionally have a real frame sequence under `assets/animations/<state>/`:

```text
assets/animations/study/frame_01.png
assets/animations/study/frame_02.png
assets/animations/study/frame_03.png
```

The same layout works for `idle`, `thinking`, `working`, `walk`, `celebrate`, `sleep`, `shiver`, `ask_hug`, `hug`, `blush`, `blush_wave`, `study`, `adjust_glasses`, `remove_glasses`, and `wave`.

Each folder supports 2–12 contiguous frames. The renderer chooses a state-specific playback speed: shiver and walk are fast, while sleep and idle are deliberately slower. Missing animations fall back to the matching static state image; missing state images fall back to `idle.png`, then `assets/tony.png`.

Temporary character actions still take precedence over long-running Agent states. A cold reaction can therefore play the shiver frames briefly and then return to `thinking` or `working` until the Agent task finishes.

The Paula-specific `blush_wave` state is separate from generic blush, so server-side persona routing can give her a distinct greeting without changing other shy interactions.
