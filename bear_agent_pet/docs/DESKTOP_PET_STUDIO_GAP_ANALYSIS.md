# Tony vs. Desktop Pet Studio: product gap analysis

Reference date: 2026-09-17

Desktop Pet Studio validates several creator-facing expectations for a modern desktop-pet product: users can bring their own animation assets, configure poses and interaction rules, attach bubbles and sounds, schedule reminders, and share packaged creations.

Tony should not copy that product's implementation. The useful product lesson is that creation must be low-friction and content must be portable. Tony's differentiator is that a pet can also have an AI brain, memory, autonomous behavior, and local tools.

## Features Tony already has

- Qt 6 / C++ desktop runtime
- drag, walk, gravity, edge behavior, cursor following, foreground-window awareness
- autonomous life state (energy, warmth, affection, loneliness, curiosity)
- response router and response packs
- long-lived local memory
- server Agent connection and local-tool bridge
- local operator logs and a training-data pipeline
- automatic update channel

## Highest-value gaps to close

### 1. Creator package format

Introduce a portable `.tonypet` package. A package contains only user-authorized pet content and declarative configuration.

Core files:

- `pet.json` - identity, version, runtime requirements, pose declarations
- `assets/states/` - single-frame poses
- `assets/animations/` - frame sequences
- `audio/` - optional sound effects
- `persona/` - optional AI/personality configuration

The package must never contain authentication tokens, pairing credentials, local memories, operator logs, API keys, or machine-specific paths.

### 2. Low-friction creator tooling

Provide a validator and packer so a non-programmer can create a pet folder, validate it, and produce one shareable file without editing C++.

### 3. Local reminders as an Agent capability

Desktop pets become more useful when they participate in the user's daily rhythm. Tony should expose reminders through the permissioned local-tool bridge rather than implementing an unrelated calendar application.

### 4. Runtime pet switching

After the package format is stable, add a Creator / Pets page to Settings where users can install, enable, disable, preview, and switch pets. Tony remains the default reference pet.

### 5. Creator ecosystem

Later releases can add a registry/store layer, ratings, signed packages, dependency/version checks, and optional cloud sync. The runtime package format should remain independent of any single store.

## Product positioning

Avoid positioning Tony as only "an AI desktop pet" or only "a desktop pet creator". The stronger framing is:

> Create a companion with your character, personality, memory, AI model, and tools.

Tony is the reference companion; Tony Runtime is the platform.
