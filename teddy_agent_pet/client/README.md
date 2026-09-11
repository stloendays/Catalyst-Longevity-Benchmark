# Tony Desktop Pet

C++20 + Qt 6 desktop client for the server-side agent. Tony is a **Teddy dog / toy poodle**, not a bear.

## V0.8 life layer

Tony now has a persistent behavior model rather than a purely random animation loop. The client tracks energy, warmth, affection, loneliness and curiosity and uses those values to bias autonomous actions such as sleeping, shivering, asking for a hug, studying, walking, stretching and yawning.

Desktop interaction is also stateful: Tony notices cursor proximity, leans toward the pointer, reacts to a head pat, distinguishes a click from drag-to-move, has a carried/landing response, and gets briefly dizzy after rough dragging. Right-click **How are you feeling?** gives a natural-language view of the current internal state without exposing game-like meters.

Agent state events still override autonomous behavior while Tony is thinking or working, so the life layer does not fight server-side tool execution.

## Character asset

Put the supplied Tony artwork at `assets/tony.png` next to the executable. New V0.8 actions gracefully fall back to the idle artwork until dedicated frames are added under `assets/states/` or `assets/animations/`.

## Build

Requires Qt 6.5+ with Widgets, WebSockets and Network.

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
```

Double-click Tony to talk to the server agent. Right-click Tony for manual actions.


## V0.8.3 desktop-world behavior

Tony now treats the Windows desktop as a physical environment rather than a flat overlay:

- walks along the active window edge when asked (and during some idle walks while perched);
- avoids newly entering the foreground window while wandering, turning back or stopping to inspect the obstacle;
- occasionally chases a fast cursor sweep that passes nearby;
- peeks from left/right/top screen edges without requiring dedicated artwork;
- falls under gravity and bounces after being dragged, with a switch to disable the physics;
- after ten quiet minutes, moves to the less intrusive bottom corner and sleeps until the next interaction.

All of these behaviors are optional from **Settings → Desktop behavior**.
