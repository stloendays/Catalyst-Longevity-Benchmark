# Tony native-motion policy

Tony keeps the original soft teddy-style poses, but movement is intentionally calm.

- Full-character state PNGs are the visual source of truth.
- Runtime never crops the source character PNG. Alpha bounds are used only to measure visible size and anchors; the complete source canvas is drawn.
- Historical `assets/animations/idle` frames are excluded from runtime. Windows frame-by-frame QA confirmed that both `frame_02` and `frame_03` can contain screenshot/background artifacts, text remnants, or incomplete character framing.
- Until a clean same-canvas blink sequence is explicitly authored and visually approved, Idle blinking is a subtle motion-only micro-expression over the complete `states/idle.png` artwork.
- Native Qt motion (small translation, rotation and scale changes) preserves the desktop-pet feel without cutting off the character.
- Animation ticks run at 110 ms.
- Micro-expression spacing is 7.5–15 seconds.
- Autonomous gestures are spaced 140–320 seconds apart.
- Walking advances one pixel every second animation tick and mirrors the artwork when Tony turns left.
- Walk, shiver, ask-hug, hug, glasses, wave, sleep and study are the preferred native-style actions.
- Every full-character pose shares the same desktop ground line so compact poses such as sleep, study and working do not appear to float.
- First launch stays non-modal. Tony starts the approval-gated device-code pairing flow after a short event-loop delay (about 2.2 seconds); blocking/manual friend-code input is reserved for explicit recovery paths.
- Hug fitting accounts for rotation and reserves additional top safety margin so the complete pose remains visible throughout motion.
- The detached always-visible settings gear was removed; settings remain available from the context menu and Ctrl+,.
- PNG integrity validation includes chunk CRC checks so corrupted artwork cannot silently ship again.
