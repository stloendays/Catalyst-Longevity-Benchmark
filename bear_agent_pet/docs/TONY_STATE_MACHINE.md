# Tony Desktop Pet v0.5 — State Machine Contract

Tony is a desktop embodiment of the server Agent. Visual behavior is driven by three sources with strict precedence:

1. **temporary persona action** — hug, shiver, blush, glasses, wave, etc.;
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
| `celebrate` | Celebrate | `celebrate.png` | successful completion | 1.6–2.2 s |
| `sleep` | Sleep | `sleep.png` | night / explicit sleep | variable |
| `shiver` | Shiver | `shiver.png` | cold-related context | 2.6–3.0 s |
| `ask_hug` | AskHug | `ask_hug.png` | Tony asks for affection | ~3.2 s |
| `hug` | Hug | `hug.png` | consensual hug accepted | ~3.0 s |
| `blush`, `blush_wave` | Blush | `blush.png` | Paula / shy context | ~2.6 s |
| `study` | Study | `study.png` | chemistry context | ~3.2–5.0 s |
| `adjust_glasses` | AdjustGlasses | `adjust_glasses.png` | focused / smart gesture | ~1.8–2.2 s |
| `remove_glasses` | RemoveGlasses | `remove_glasses.png` | no-glasses handsome mode | ~3.0–3.5 s |
| `wave` | Wave | `wave.png` | startup / hover greeting | ~1.2–1.7 s |

Missing artwork is legal. The renderer falls back to `idle.png`, then `assets/tony.png`, while retaining motion behavior.

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
- right click: ask, hug, connect/pair, local SSH, walk, think, study, glasses, no-glasses, celebrate, cold, sleep, quit;
- tray click: restore/raise Tony.

## Artwork geometry

Production artwork should use the same transparent square canvas and the same visual anchor. The visible foot baseline and head center should remain stable across states. For the current 230 x 250 window, Tony is rendered into a nominal 200 x 200 character box. Source artwork may be larger; the build/package can downsample for runtime use.

Recommended source master: 1024–1254 px transparent PNG. Recommended runtime asset: 220–512 px transparent PNG depending package-size target.

## Next animation layer

The current renderer combines a state image with procedural translation, rotation and scale. The next compatible extension is multi-frame state artwork using filenames such as `idle_00.png`, `idle_01.png`, `idle_02.png`. The state-machine contract above should remain unchanged; only the renderer should gain per-state frame playback.
