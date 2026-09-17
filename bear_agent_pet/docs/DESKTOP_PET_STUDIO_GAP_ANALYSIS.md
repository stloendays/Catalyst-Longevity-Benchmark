# Tony vs. Desktop Pet Studio: product gap analysis

Reference date: 2026-09-17

Desktop Pet Studio validates several creator-facing expectations for a modern desktop-pet product: users can bring their own animation assets, configure poses and interaction rules, attach bubbles and sounds, schedule reminders, and share packaged creations.

Tony should not copy that product's implementation. The useful product lesson is that creation must be low-friction and content must be portable. Tony's differentiator is that a pet can also have an AI brain, memory, autonomous behavior, and permissioned local tools.

## Features Tony already has

- Qt 6 / C++ desktop runtime
- drag, walk, gravity, edge behavior, cursor following, foreground-window awareness
- autonomous life state (energy, warmth, affection, loneliness, curiosity)
- response router and response packs
- long-lived local memory
- server Agent connection and local-tool bridge
- local operator logs and a training-data pipeline
- automatic update channel

## Tony 1.1 creator foundation implemented

### 1. Portable `.tonypet` package

A content-only package now has a declared manifest and validation/packing tool. Core files are:

- `pet.json` - identity, version, rights confirmation, presentation and pose declarations
- `states/` - single-frame poses
- `animations/` - optional frame sequences
- `config/` - optional response/personality data referenced by the manifest

The package validator rejects executables, scripts, credentials, certificates, tokens, memories, operator logs, unsafe paths and other machine-private content.

### 2. Graphical creator entry point

Tony's tray exposes **Pets & Creator…**. A user can choose a creator folder, validate its manifest and idle artwork, preview the pet immediately, persist it for the next launch, and restore built-in Tony without restarting.

This deliberately starts with the visual body. AI/personality switching is kept as a separate future package layer so untrusted artwork cannot silently redefine local permissions.

### 3. Local reminders

Tony now has a persistent local reminder manager plus **Quick Reminder…** in the tray. The Agent protocol also supports reminder requests through the existing permissioned local-tool bridge. The Windows client independently validates and confirms Agent-requested operations.

### 4. Safer creator/runtime contract

A creator pet must have a safe static `poses.idle.state` fallback. Runtime paths are canonicalized and constrained to the selected creator folder. Invalid packages do not replace the current pet.

### 5. Expanded regression gate

CI is defined to compile the full desktop target, execute C++ core tests, syntax-check the gateway, test reminder/tool routing, and round-trip the reference `.tonypet` package.

## Highest-value gaps that remain

### Per-pet brain and personality profiles

Allow optional persona/response/autonomy packs to become active only after explicit capability validation. Model credentials and user memories must remain runtime-owned rather than portable-pet content.

### Rich creator editor

Move from selecting an already prepared folder to a guided editor that can:

- create a pet project from a template;
- assign images to poses visually;
- preview animation timing and mirroring;
- edit bubbles/sounds/size without hand-editing JSON;
- validate before export.

### Direct `.tonypet` install

The current runtime previews unpacked creator folders. Add signed/validated package import into a managed local pet library, then expose install/update/uninstall operations in the Creator UI.

### Creator ecosystem

Later releases can add a registry/store layer, ratings, signed packages, dependency/version checks, optional cloud sync, and creator publishing. The package/runtime format should remain independent of any single store.

## Product positioning

Avoid positioning Tony as only "an AI desktop pet" or only "a desktop pet creator". The stronger framing is:

> Create a companion with your character, personality, memory, AI model, and tools.

Tony is the reference companion; Tony Runtime is the platform.
