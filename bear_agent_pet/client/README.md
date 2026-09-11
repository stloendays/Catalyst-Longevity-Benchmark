# Tony Desktop Pet

C++20 + Qt 6 desktop client for the server-side OpenClaw agent.

The visible pet name is **Tony**. The desktop behavior layer currently provides idle breathing, bob/working, walking, thinking, celebrating, sleeping, drag-to-move, persistent position, tray presence, double-click chat and a right-click action menu. Agent state events can drive the same animations.

## Character asset

Put the supplied Tony artwork at `assets/tony.png` next to the executable (or `tony.png` next to the executable). The client intentionally keeps the behavior engine independent from the artwork so later action frames/sprite sheets can replace the single static image without changing the protocol.

## Build

Requires Qt 6.5+ with Widgets, WebSockets and Network.

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
```

Double click Tony to talk to the server agent. Right click Tony for manual actions.
