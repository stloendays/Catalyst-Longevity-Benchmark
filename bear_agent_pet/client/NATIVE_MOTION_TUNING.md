# Tony native-motion policy

Tony keeps the original soft teddy-style poses, but movement is intentionally calm.

- Full-character state PNGs are the visual source of truth.
- Legacy multi-frame sequences are not used as full sprites when they contain cropped bodies or inconsistent framing.
- Native Qt motion (small translation, rotation and scale changes) preserves the old desktop-pet feel without cutting off the character.
- Animation ticks run at 110 ms.
- Micro-expression spacing is 7.5–15 seconds.
- Autonomous gestures are spaced 140–320 seconds apart.
- Walking advances one pixel every second animation tick.
- Walk, shiver, ask-hug, hug, glasses, wave, sleep and study are the preferred native-style actions.
- PNG integrity validation includes chunk CRC checks so corrupted artwork cannot silently ship again.
