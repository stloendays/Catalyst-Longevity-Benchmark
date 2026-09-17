# Tony Desktop Pet

Tony is a C++20 + Qt 6 Windows desktop companion connected to the server-side OpenClaw agent. The built-in reference character is **Tony**, a Teddy / toy-poodle-style pet.

Tony is also evolving into an **Agent Pet Runtime**: creators can prepare their own pet artwork and declarative `pet.json`, validate/package it as `.tonypet`, and preview the pet in the same desktop runtime while keeping Tony as the safe default.

## Current capabilities

- transparent always-on-desktop pet window;
- idle, walking, thinking, studying, sleeping, shivering, hugging and other character states;
- drag-to-move with persistent desktop position;
- tray presence, double-click chat and right-click actions;
- HTTPS/WSS agent connectivity with pairing and reconnect behavior;
- local memory, response routing and autonomous companion behavior;
- permissioned local tools, including persistent local reminders;
- local operator logging;
- packaged artwork/state validation;
- versioned Windows packaging and automatic update support;
- creator-facing `.tonypet` validation, packing and safe unpacking;
- custom-pet preview from a local creator folder.

## Runtime assets

Tony's built-in runtime artwork has one canonical filesystem pipeline:

- static poses: `assets/states/*.png`
- frame animations: `assets/animations/<action>/frame_*.png`
- application icon: `assets/tony-app.ico`
- response/config packs: `assets/config/*.json`
- Agent Pet manifest: `assets/pet.json`

The client falls back to the pet's idle state when a custom package does not provide every semantic pose. New creator packages should use the same canonical pose names where possible (`idle`, `walk`, `thinking`, `sleep`, `hug`, `wave`, and so on).

## Create and validate a pet

A creator folder must contain `pet.json` plus the artwork referenced by that manifest. The built-in Tony assets are the reference implementation.

Validate a pet folder:

```powershell
python tools/tonypet.py validate C:\path\to\MyPet
```

Build a portable package:

```powershell
python tools/tonypet.py pack C:\path\to\MyPet C:\path\to\MyPet.tonypet
```

The packer deliberately excludes executable code, credentials, tokens, memories, operator logs and other machine-private content. `rights.confirmed` must be true before packaging.

### Preview a creator pet

Until the graphical Creator page is added, a local creator folder can be selected directly:

```powershell
TonyDesktopPet.exe --pet-root=C:\path\to\MyPet
```

That selection is persisted for later starts. Return to built-in Tony with:

```powershell
TonyDesktopPet.exe --pet-reset
```

An invalid custom manifest never replaces the built-in Tony artwork; the runtime keeps the built-in assets as the startup fallback.

## Local reminders

Tony's local bridge supports permissioned reminder tools in addition to clipboard, screenshot, URL/file opening and notifications:

- `create_reminder`
- `list_reminders`
- `cancel_reminder`

Reminders are persisted in the local Qt settings store, fire through the Windows tray, and are never packaged into `.tonypet` files. The client still asks for confirmation when the server Agent wants to create, read or cancel reminder data.

The gateway recognizes simple explicit phrases such as `remind me in 20 minutes to stretch` and `20分钟后提醒我喝水`, while absolute reminders can use ISO 8601 timestamps.

## Build

Requires Qt 6.5+ with Widgets, WebSockets and Network.

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
```

Run the core regression tests with:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Double-click Tony to talk to the agent. Right-click Tony for manual actions and settings.
