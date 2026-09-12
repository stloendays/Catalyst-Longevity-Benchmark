# Tony branch convergence policy

## Canonical line

Tony desktop-pet development is converged on the v0.9.8 product tree. The active shared development branch and the v0.9.8 network branch must advance from the same converged commit.

Current converged line includes the histories of:

- `feature/bear-agent-pet`
- `fix/tony-v097-idle-blink`
- `fix/tony-post095-visual-regressions`
- `fix/tony-v098-icon-connection`

Older QA, staging, temporary and recovery branches are historical references only. Do not cherry-pick their integration workflows back into the current product tree unless a specific missing change has first been verified against the current source.

## Rules for parallel GPT / human development

1. Start every new task from the current `feature/bear-agent-pet` tip.
2. Use a dedicated task branch. Do not develop directly on the shared branch.
3. Before integration, compare the task branch with the latest shared tip and rebase or merge as appropriate.
4. Integrate through a reviewed merge/PR or an explicitly verified fast-forward.
5. Never force-push an active shared branch to resolve divergence.
6. If another session moved the shared tip, stop the automatic integration and re-check the diff rather than overwriting it.

## Visual asset ownership

Runtime artwork has one canonical filesystem pipeline:

- static poses: `bear_agent_pet/client/assets/states/*.png`
- frame animations: `bear_agent_pet/client/assets/animations/<action>/frame_*.png`
- application icon: `bear_agent_pet/client/assets/tony-app.ico`

`PetWindowV7` loads those packaged filesystem assets. Missing authored actions must fall back explicitly to approved Tony states rather than silently introducing a second runtime asset system.

The `EmbeddedActionAssets*` sources are legacy/incomplete staging material and are not part of the current CMake runtime target. Preserve them only as recovery evidence until their usable frames are migrated into `assets/animations/`; do not wire them in as a parallel loader.

## v0.9.8 convergence invariant

Convergence work must preserve the validated public connection path and pairing behavior. Visual/resource cleanup must not regress HTTPS pairing, authenticated WSS, reconnect/backoff, DPAPI token storage, or the current Windows packaging path.
