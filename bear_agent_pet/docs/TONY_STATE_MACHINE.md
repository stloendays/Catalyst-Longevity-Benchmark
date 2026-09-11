# Tony Desktop Pet v0.6 — State Machine Contract

Tony is the desktop embodiment of the server Agent. Visual behavior is driven by three sources with strict precedence:

1. **temporary persona action** — hug, shiver, Paula blush, glasses, wave, etc.;
2. **long-running Agent phase** — thinking, working/tool-running, error, sleeping;
3. **passive desktop idle behavior** — breathing, short walk, wave, asking for a hug, getting cold.

A temporary action owns the avatar until its `duration_ms` expires. The client then restores the action implied by the current long-running Agent state instead of blindly returning to idle.

## Canonical states

| Wire action/state | Client action | Artwork key | Typical trigger | Default duration |
|---|---|---|---|---:|
| `idle` | Idle | `idle.png` | no active task | persistent |
| `thinking` | Think | `thinking.png` | request accepted / reasoning | persistent |
| `working`, `tool_running` | Bob | `working.png` | tool execution / server work | persistent |
| `walk` | Walk | `walk.png` | passive desktop wandering | 4–7 s |
| `celebrate` | Celebrate | `celebrate.png` | successful completion | 1.8–2.4 s |
| `sleep` | Sleep | `sleep.png` | night / explicit sleep | variable |
| `shiver` | Shiver | `shiver.png` | cold-related context | 2.6–3.0 s |
| `ask_hug` | AskHug | `ask_hug.png` | Tony asks for affection | ~3.4 s |
| `hug` | Hug | `hug.png` | consensual hug accepted | ~3.0 s |
| `blush` | Blush | `blush.png` | generic shy context | ~2.6 s |
| `blush_wave` | BlushWave | `blush_wave.png` | Paula-related shy greeting | ~3.6 s |
| `study` | Study | `study.png` | chemistry context | ~3.2–5.0 s |
| `adjust_glasses` | AdjustGlasses | `adjust_glasses.png` | focused / smart gesture | ~1.8–2.2 s |
| `remove_glasses` | RemoveGlasses | `remove_glasses.png` | no-glasses handsome mode | ~3.0–3.5 s |
| `wave` | Wave | `wave.png` | startup / hover greeting | ~1.2–1.8 s |

Missing artwork is legal. The renderer falls back to `idle.png`, then `assets/tony.png`, while retaining motion behavior.

## Multi-frame animation

v0.6 adds real frame playback without changing the Agent wire protocol. A state may optionally provide a contiguous frame sequence:

```text
assets/animations/<state>/
  frame_01.png
  frame_02.png
  frame_03.png
  ...
```

The first animated states are `idle`, `ask_hug`, `shiver`, and `walk`. If a sequence is missing, Tony automatically uses the matching static state image. The renderer combines frame playback with a small amount of translation/rotation/scale so transitions still feel soft.

Current playback cadence:

- `idle`: slow, about 480 ms per frame;
- `ask_hug`: about 240 ms per frame;
- `shiver`: about 80 ms per frame;
- `walk`: about 160 ms per frame.

Animation folders must contain 2–12 contiguous PNG files named `frame_01.png`, `frame_02.png`, etc. GitHub Actions validates both state images and frame sequences.

## Agent event sequence

Recommended server sequence for one request:

```text
client message
  -> avatar_action (contextual short reaction)
  -> agent_state: thinking
  -> agent_state: working / tool_running (optional)
  -> text_delta ...
  -> final
  -> avatar_action (completion reaction)
  -> agent_state: idle
```

The text bubble stays open while `text_delta` events stream. `final` starts the normal dismiss timeout.

## Passive behavior

Passive behavior runs only when Tony is idle, is not being dragged and no Agent task is active. The interval is randomized between 18 and 46 seconds so Tony does not feel clock-driven. Candidate moments include shivering, asking for a hug, walking, thinking, waving and adjusting glasses. Between 23:00 and 07:00 local time, a short sleep behavior is also eligible.

Passive behavior must never interrupt a technical task, confirmation dialog, pairing flow or explicit user action.

## Desktop interaction

- hover: short paw wave when idle;
- left drag: move Tony and persist the new position;
- double click: open the compact ask dialog;
- right click: ask, hug, connect/pair, local SSH, walk, think, study, glasses, no-glasses, Paula greeting, celebrate, cold, sleep, quit;
- tray click: restore/raise Tony.

The `Paula 来了` local preview action maps to `blush_wave` so the Paula-specific animation can be tested without waiting for a server response.

## Artwork geometry

Production artwork should use the same transparent square canvas and the same visual anchor. The visible foot baseline and head center should remain stable across states. For the current 230 x 250 window, Tony is rendered into a nominal 200 x 200 character box.

Recommended source master: 1024–1254 px transparent PNG. Recommended runtime state asset: 220–512 px. Animation frames may be 220–320 px to keep the Windows package compact.

## Next layer

The next animation pass should add dedicated frame sequences for `sleep`, `study`, `remove_glasses`, `celebrate`, and `blush_wave`, then replace the current standard `QInputDialog` with Tony's own floating input bubble.
