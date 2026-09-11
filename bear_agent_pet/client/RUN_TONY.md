# Tony Desktop Pet v0.9.8 — Windows Quick Start

Tony is a C++20 + Qt 6 desktop companion. The normal Windows path now connects securely to the Tony Gateway over HTTPS/WSS and receives streamed replies from the server-side Qwen backend.

## Run

Unzip the Windows package and start `TonyDesktopPet.exe`. Keep the packaged `assets` directory beside the executable.

Tony is frameless, transparent and always on top. Drag him with the left mouse button, double-click to chat, or right-click for actions such as hug, walk, study chemistry, adjust glasses, no-glasses mode, celebrate, shiver and sleep.

## Default secure connection

Tony v0.9.8 uses this public endpoint by default:

```text
wss://150.158.27.206/agent/ws
```

The matching HTTPS API is:

```text
https://150.158.27.206
```

The server presents a publicly trusted TLS certificate for the IP address. The internal Gateway and Qwen ports remain bound to localhost and are not exposed directly to the Internet.

## First-time device pairing

If this Windows account has no saved device token, Tony automatically performs the device-code flow:

```text
Tony on Windows
  -> HTTPS POST /pair/request
  -> shows a short device code
  -> HTTPS polls /pair/status
  -> owner approves the code on the server
  -> Tony receives a per-device bearer token
  -> WSS /agent/ws with Authorization: Bearer <device token>
  -> Qwen streams text_delta events
  -> Tony displays the final reply
```

Approve a device code on the server with:

```bash
cd /home/ubuntu/.local/share/bear-agent/app
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli approve 'XXXX-XXXX'
```

The device code is short-lived. Do not publish a live pairing code.

After approval, Tony stores the device token in Windows settings protected with DPAPI and reconnects automatically on later launches. The server stores token verification data rather than relying on a shared desktop password.

## Re-pairing

Open Tony Settings and choose `Test connection / pair this computer…` when the saved device credential is missing, revoked, or you intentionally want a fresh pairing.

## Owner/development SSH fallback

An SSH tunnel remains available as an owner/development fallback, but it is no longer the normal public connection path. The public HTTPS/WSS path should be used for ordinary Windows installations.

## Chat behavior

Double-click Tony to open the compact chat composer. After submission, Tony sends the message through the authenticated WSS connection and streams the model reply into his speech bubble.

```text
Double-click Tony
  -> compact input card
  -> Enter / Send
  -> WSS message
  -> Qwen streaming reply
  -> Tony speech bubble
```

## Artwork and animation fallback

Static state images live under `assets/states/`. Optional multi-frame animations live under `assets/animations/<state>/`. Missing animation frames fall back to the matching static state image; missing state images fall back to `idle.png`, then `assets/tony.png`.

Canonical states include idle, curious, pet, carried, landing, dizzy, stretch, yawn, working, walk, thinking, celebrate, sleep, shiver, ask-hug, hug, blush, Paula-specific blush-wave, study, adjust-glasses, remove-glasses and wave.
