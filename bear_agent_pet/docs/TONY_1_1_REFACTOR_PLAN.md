# Tony 1.1 refactor plan

This document defines the low-risk path from the current Tony 1.0.9 product line to a cleaner Tony 1.1 architecture.

## Phase 0 — safety net

Status: in progress.

- Add deterministic tests for `TonyBehaviorEngine`.
- Add deterministic tests for `TonyMemoryStore`.
- Run core tests in GitHub Actions.
- Keep visible behavior unchanged while the safety net is introduced.

Exit condition: core tests are green on the canonical Tony branch.

## Phase 1 — repository convergence

- Move the catalyst / software-copyright product into `stloendays/Copyright` only after the destination contains the migrated project, not merely a placeholder README.
- Make Tony the default product on this repository's `main` branch.
- Preserve an archive tag/branch for the pre-split mixed repository.
- Remove catalyst-only workflows from the Tony repository after the destination migration is verified.
- Keep Tony server/model/deployment workflows that are still used by the desktop pet.

Exit condition: cloning the default branch gives a Tony-only project, while the catalyst project remains independently usable from `Copyright`.

## Phase 2 — naming cleanup

Target layout after repository convergence:

```text
tony/
  client/
  gateway/
  training/
  operator_logs/
  docs/
```

- Rename `bear_agent_pet` to `tony` or `tony_desktop_pet` in one dedicated migration.
- Update workflow paths, scripts, documentation and packaging paths atomically.
- Do not mix this rename with behavioral changes.

Exit condition: active source/config paths no longer use Bear naming for Tony.

## Phase 3 — PetWindow decomposition

The active window implementation currently carries historical version naming and too many responsibilities.

Target responsibility-based components:

```text
PetWindow
PetRenderer
PetAnimationController
PetInteractionController
PetChatController
PetPositionController
PetContextMenu
PetStateController
```

Rules:

- no new `PetWindowV8`, `PetWindowV9`, etc.;
- move one responsibility at a time;
- keep the public behavior unchanged during extraction;
- delete legacy/versioned implementations only after the replacement has passed build and smoke tests.

Exit condition: the active product no longer depends on version-named window source files or text-including another `.cpp` implementation.

## Phase 4 — updater rollback

The updater already verifies the package hash before installation. Tony 1.1 should add transactional recovery:

1. stage and validate the new package;
2. snapshot the managed files owned by the current installation;
3. install the new managed package;
4. start the new executable with a pending-health marker;
5. let the new process write a healthy marker after startup initialization succeeds;
6. if health is not confirmed, restore the previous managed snapshot and restart the prior version;
7. keep rollback metadata bounded to one previous version.

The rollback path must never delete unmanaged user files in the installation directory.

Exit condition: an intentionally broken update can automatically return to the last known-good Tony package.

## Phase 5 — structured memory and interaction data

Evolve memory/logging without exposing raw private conversation data by default.

Suggested memory fields:

```text
type
key
value
confidence
source
created_at
last_used_at
```

Suggested event classes:

```text
user_message
assistant_reply
route_selected
model_selected
action_selected
memory_written
connection_lost
connection_restored
update_started
update_completed
```

Training/export tooling should consume an explicitly selected, sanitized dataset rather than reading the live local store directly.

## Phase 6 — behavior and animation expansion

After architecture stabilization:

- convert high-value static poses into short authored animations;
- add explicit transitions between common states;
- allow autonomous behavior to use time, idle duration, interaction history and internal needs;
- keep user controls for autonomous behavior intensity;
- keep server/model routing separate from animation selection.

## Invariants through all phases

The following must not regress during refactoring:

- HTTPS pairing;
- authenticated WSS connectivity;
- reconnect/backoff;
- DPAPI-protected token storage on Windows;
- local logging;
- Windows packaging;
- automatic update verification;
- Tony's approved Teddy / toy-poodle visual identity;
- separation between runtime response packs and future model-training corpora.
