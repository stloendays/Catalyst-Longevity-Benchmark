# Tony branch convergence policy

## Canonical line

Tony Desktop Pet is currently converged on the **1.0.9** product line. Until the repository split is completed and Tony becomes the default `main` product, the canonical Tony product branch is:

- `feature/tony-v109-full-canvas-render`

New work must start from the current canonical Tony tip and use a dedicated task branch. The legacy `feature/bear-agent-pet` branch is no longer the authoritative starting point for new work.

## Rules for parallel GPT / human development

1. Start every new task from the current canonical Tony product branch.
2. Use a dedicated task branch; do not develop directly on the shared product branch.
3. Before integration, compare the task branch with the latest shared tip and rebase or merge as appropriate.
4. Integrate through a reviewed pull request or an explicitly verified fast-forward.
5. Never force-push an active shared branch to resolve divergence.
6. If another session moved the shared tip, stop automatic integration and re-check the diff rather than overwriting it.
7. After Tony becomes the repository default product, move the canonical line to `main` and keep feature branches short-lived.

## Visual asset ownership

Runtime artwork has one canonical filesystem pipeline:

- static poses: `bear_agent_pet/client/assets/states/*.png`
- frame animations: `bear_agent_pet/client/assets/animations/<action>/frame_*.png`
- application icon: `bear_agent_pet/client/assets/tony-app.ico`
- response/config packs: `bear_agent_pet/client/assets/config/*.json`

`PetWindowV7` currently loads the packaged filesystem assets. Missing authored actions must fall back explicitly to approved Tony states rather than silently introducing a second runtime asset system.

The `EmbeddedActionAssets*` sources are legacy/incomplete staging material and are not part of the current CMake runtime target. Preserve them only until their remaining usable content is either migrated or explicitly discarded during the 1.1 refactor.

## 1.1 refactor targets

The next architecture pass should remove version-named window implementations from the active design and converge toward responsibility-based components such as rendering, animation, interaction, chat and state control. Core deterministic behavior and memory logic must be covered by automated tests before larger window refactors are merged.

## Connectivity invariant

Refactoring must preserve the validated public connection path and pairing behavior. Visual/resource cleanup must not regress HTTPS pairing, authenticated WSS, reconnect/backoff, DPAPI token storage, Windows packaging, or automatic update behavior.
