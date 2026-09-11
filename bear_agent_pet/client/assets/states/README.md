# Tony state artwork contract

The Qt client loads an optional transparent PNG for each state below. Missing files automatically fall back to `assets/tony.png`, so artwork can be replaced incrementally without changing C++ code.

Expected files:

- `idle.png` — neutral glasses-on Tony
- `working.png` — focused / active Agent state
- `walk.png` — walking pose
- `thinking.png` — thinking / head-scratch pose
- `celebrate.png` — happy completion pose
- `sleep.png` — sleeping pose
- `shiver.png` — cold / tucked-in pose
- `ask_hug.png` — arms partly open, asking before contact
- `hug.png` — warm consensual hug pose
- `blush.png` — bashful Paula-related pose
- `study.png` — chemistry study pose with glasses
- `adjust_glasses.png` — glasses-adjusting pose
- `remove_glasses.png` — no-glasses confident pose
- `wave.png` — greeting / paw wave pose

Artwork requirements:

- transparent PNG
- square canvas, recommended 1024 x 1024 source
- same body scale and anchor point across all states
- no text baked into the image
- keep Tony visually recognizable across states
- avoid large shadows outside the character bounds

The desktop app still applies lightweight translation, rotation and scale animation on top of each state image.
