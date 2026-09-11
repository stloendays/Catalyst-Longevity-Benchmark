# Tony state artwork contract

The Qt client loads transparent PNG artwork for Tony's states and optional multi-frame animations. Missing files are legal and fall back to `assets/tony.png`, so artwork can be replaced incrementally without changing the Agent protocol.

Expected state files:

- `idle.png` — neutral glasses-on Tony
- `working.png` — focused / active Agent state
- `walk.png` — walking pose
- `thinking.png` — thinking pose
- `celebrate.png` — happy completion pose
- `sleep.png` — sleeping pose
- `shiver.png` — cold / tucked-in pose
- `ask_hug.png` — paws open, asking before contact
- `hug.png` — warm consensual hug pose
- `blush.png` — generic bashful pose
- `blush_wave.png` — Paula-specific shy greeting pose
- `study.png` — chemistry study pose with glasses
- `adjust_glasses.png` — glasses-adjusting pose
- `remove_glasses.png` — no-glasses confident pose
- `wave.png` — greeting / paw wave pose

Optional animation sequences live beside the state directory:

```text
assets/animations/idle/frame_01.png
assets/animations/idle/frame_02.png
assets/animations/idle/frame_03.png

assets/animations/ask_hug/frame_01.png
...
```

v0.6 plays real frame sequences for `idle`, `ask_hug`, `shiver`, and `walk`. Each animation folder must contain 2–12 contiguous frames named `frame_01.png`, `frame_02.png`, etc. A missing animation folder simply falls back to the static state image.

Artwork requirements:

- transparent PNG
- square canvas
- same body scale and foot baseline across frames in one animation
- no large baked-in text labels
- Tony remains visually recognizable across every state
- runtime state art: recommended 220–512 px
- runtime animation frames: recommended 220–320 px

The desktop app still applies lightweight translation, rotation and scale on top of the state/frame artwork.
