# Tony Desktop Pet

Tony is a C++20 + Qt 6 Windows desktop companion connected to the server-side OpenClaw agent. The visible character is **Tony**, a Teddy / toy-poodle-style pet.

## Current capabilities

- transparent always-on-desktop pet window;
- idle, walking, thinking, studying, sleeping, shivering, hugging and other character states;
- drag-to-move with persistent desktop position;
- tray presence, double-click chat and right-click actions;
- HTTPS/WSS agent connectivity with pairing and reconnect behavior;
- local memory, response routing and autonomous companion behavior;
- local operator logging;
- packaged artwork/state validation;
- versioned Windows packaging and automatic update support.

## Runtime assets

Tony's runtime artwork has one canonical filesystem pipeline:

- static poses: `assets/states/*.png`
- frame animations: `assets/animations/<action>/frame_*.png`
- application icon: `assets/tony-app.ico`
- response/config packs: `assets/config/*.json`

The client should fall back to an approved Tony state when an authored animation is unavailable. New runtime artwork should be added to this asset pipeline instead of introducing a second loader.

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
