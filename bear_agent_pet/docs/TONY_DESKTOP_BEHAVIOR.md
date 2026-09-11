# Tony Desktop Behavior V0.8.1

Tony is a Teddy dog / toy poodle desktop companion, not a bear. The desktop itself is part of his environment.

## Implemented behavior

- Drag-release settling against the active screen work area.
- Optional snap to bottom/top/left/right screen surfaces. The bottom surface naturally sits above a bottom taskbar.
- Taskbar-aware home action: infer the taskbar side from the difference between `QScreen::geometry()` and `availableGeometry()`.
- Side-edge resting pose: Tony leans inward rather than behaving like a floating window.
- Multi-monitor-safe walking with direct crossing to adjacent displays when their vertical spans overlap.
- Manual “Move to next display” action.
- Periodic visibility recovery after monitor removal, resolution changes, RDP sessions, or docking/undocking a laptop.
- Low-frequency cursor curiosity: Tony may walk a short distance toward a pointer that has been resting nearby, gated by energy and curiosity.
- User controls for cursor following and edge/taskbar snapping.
- Dock mode and display name persist across restarts.

## Safety / usability constraints

Desktop movement never runs while Tony is being dragged, while the chat composer is open, or while the Agent is busy. Cursor-follow behavior is deliberately probabilistic and short so Tony does not constantly chase the pointer.
