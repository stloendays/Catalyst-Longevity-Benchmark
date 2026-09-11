# Tony frame animations

Tony v0.6.1 can play frame sequences for every canonical avatar state.

Use one directory per state:

```text
assets/animations/
  idle/frame_01.png
  idle/frame_02.png
  idle/frame_03.png
  thinking/frame_01.png
  thinking/frame_02.png
  working/frame_01.png
  ...
```

Supported directories:

`idle`, `working`, `walk`, `thinking`, `celebrate`, `sleep`, `shiver`, `ask_hug`, `hug`, `blush`, `blush_wave`, `study`, `adjust_glasses`, `remove_glasses`, `wave`.

Rules:

- frames are named `frame_01.png`, `frame_02.png`, ... with no gaps;
- 2–12 frames per sequence;
- all frames in one sequence use the same square transparent canvas;
- keep Tony's foot baseline and head center stable across frames;
- the renderer prefers a frame sequence, then `assets/states/<state>.png`, then `assets/states/idle.png`, then `assets/tony.png`;
- frame timing is selected by action: shivering and walking are faster, sleeping and idle are slower.

`blush_wave` is the dedicated Paula reaction and should remain visually distinct from ordinary `blush`.
