# Tony Desktop Pet

Tony is a C++20 + Qt 6 Windows desktop companion connected to the server-side agent gateway. The built-in reference character is **Tony**, a Teddy / toy-poodle-style pet.

Tony is also evolving into an **Agent Pet Runtime**: creators can prepare their own pet artwork and declarative `pet.json`, validate/package it as `.tonypet`, and preview the pet in the same desktop runtime while keeping Tony as the safe default.

## Tony 1.1.0 UX refresh

Tony 1.1.0 focuses on making the existing Agent Pet features easier to discover and less disruptive:

- the saved English / 简体中文 interface preference is respected across launches;
- a first-run guide explains click, double-click, drag, right-click and tray interactions;
- left-clicking Tony's tray icon opens chat immediately;
- the tray menu puts chat, hug and status before configuration actions;
- the chat composer has clearer keyboard guidance and disables Send for empty messages;
- automatic updates may check and download in the background, but Tony no longer restarts itself without the user choosing when to install.

## Current capabilities

- transparent always-on-desktop pet window;
- idle, walking, thinking, studying, sleeping, shivering, hugging and other character states;
- drag-to-move with persistent desktop position;
- tray presence, double-click chat and right-click actions;
- HTTPS/WSS agent connectivity with pairing and reconnect behavior;
- local memory, response routing and autonomous companion behavior;
- permissioned local tools, including persistent local reminders;
- local operator logging;
- packaged artwork/state validation;
- versioned Windows packaging and automatic update support;
- creator-facing `.tonypet` validation, packing and safe unpacking;
- graphical **Pets & Creator** flow for selecting, validating, previewing and resetting custom pets;
- custom-pet preview from either the GUI or a local creator folder on the command line.

## Pets & Creator

Open Tony's tray menu and choose **Pets & Creator…**. A creator can select a folder containing `pet.json` and transparent artwork. Tony validates identity, rights confirmation and the idle fallback before changing the visible desktop pet. Invalid packages do not replace the current pet.

The current creator layer changes the pet's visual body and presentation while Tony Runtime continues to provide AI, memory, autonomy and local tools. Per-pet brain/personality switching is the next layer of the package format rather than being hidden inside image assets.

To restore the built-in reference character, choose **Restore Tony** in the Creator window.

## Runtime assets

Tony's built-in runtime artwork has one canonical filesystem pipeline:

- static poses: `assets/states/*.png`
- frame animations: `assets/animations/<action>/frame_*.png`
- application icon: `assets/tony-app.ico`
- response/config packs: `assets/config/*.json`
- Agent Pet manifest: `assets/pet.json`

The client falls back to the pet's idle state when a custom package does not provide every semantic pose. New creator packages should use the same canonical pose names where possible (`idle`, `walk`, `thinking`, `sleep`, `hug`, `wave`, and so on).

## Create and validate a pet

A creator folder must contain `pet.json` plus the artwork referenced by that manifest. The built-in Tony assets are the reference implementation. `poses.idle.state` is required so every pet always has a safe static fallback.

Validate a pet folder:

```powershell
python tools/tonypet.py validate C:\path\to\MyPet
```

Build a portable package:

```powershell
python tools/tonypet.py pack C:\path\to\MyPet C:\path\to\MyPet.tonypet
```

The packer deliberately excludes executable code, credentials, tokens, memories, operator logs and other machine-private content. `rights.confirmed` must be true before packaging.

For development/automation, a local creator folder can also be selected directly:

```powershell
TonyDesktopPet.exe --pet-root=C:\path\to\MyPet
```

Return to built-in Tony with:

```powershell
TonyDesktopPet.exe --pet-reset
```

## Tony 1.1.2 UX polish

- Tony is single-instance: launching the executable again activates the existing pet and opens chat instead of creating a duplicate pet.
- A temporary **Do Not Disturb** mode can silence proactive speech and automatic news for 1 hour, 4 hours, or until 08:00 without changing the user's saved autonomy/news preferences.
- News Companion settings are consolidated into one dialog for enable/disable, cadence, quiet hours, source selection, manual fetch, and opening the latest original story.
- Manual interactions, chat, reminders, and manual news checks still work during Do Not Disturb.
## News Companion

Tony can optionally fetch RSS/Atom headlines from the internet and surface them as calm, source-attributed briefings.

Open the tray menu and choose **News Companion**:

- **Automatic headlines** enables/disables background news;
- **Tell me one now** fetches an unseen headline immediately;
- **Open latest story** opens the original HTTPS article in the browser;
- **Sources** enables/disables individual packaged feeds;
- **Frequency** limits automatic briefings to at most once per 1, 2, or 4 hours.

Automatic news is opt-in and defaults to off. When enabled, Tony:

- uses HTTPS RSS/Atom feeds defined in `assets/config/news_sources.json`;
- avoids 23:00–08:00 by default;
- waits when Tony is already chatting, running a tool, being dragged, showing another bubble, or the user has just interacted;
- stores only small local deduplication keys so the same headline is not repeated;
- keeps the original article URL available through **Open latest story**;
- treats remote headlines as untrusted data before sending them to the Agent for a one-sentence summary;
- instructs the Agent to stay factual and neutral and not invent details beyond the headline.

The built-in source file currently includes BBC World, BBC Technology, NASA, and NASA JPL. The source list is declarative so future creator/user-defined feed management can be added without changing the fetch engine.
## Local reminders

Tony has a persistent local reminder manager. Reminders live in the local Qt settings store, fire through the desktop tray, and are never included in `.tonypet` packages.

There are two user paths:

1. choose **Quick Reminder…** in Tony's tray and create a reminder entirely on the local PC;
2. ask Tony naturally, for example `remind me in 20 minutes to stretch` or `20分钟后提醒我喝水`.

For the Agent path, the gateway emits only a whitelisted `tool_request`; the Windows client independently validates it and asks the user for confirmation before creating, reading or cancelling reminder data. The local bridge supports:

- `create_reminder`
- `list_reminders`
- `cancel_reminder`

The gateway never receives arbitrary shell access from this mechanism. Existing local-tool policy still excludes shell/PowerShell execution, arbitrary file writes, file deletion and similar unrestricted operations.

## Build

Requires Qt 6.5+ with Widgets, WebSockets and Network.

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
```

Run the regression tests with:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

The repository CI additionally builds the full desktop target and runs gateway syntax checks plus local-tool routing tests.

Double-click Tony to talk to the agent. Right-click Tony for manual actions and settings.
