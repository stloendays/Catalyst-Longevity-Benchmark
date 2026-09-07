# Catalyst Longevity Research — Native Desktop

This directory contains the native Windows desktop migration of Catalyst Longevity Research.

## Technology

- C++20
- Qt 6 Widgets
- CMake
- Native Windows executable deployment with `windeployqt`
- Inno Setup installer

The native application does not start a browser and does not require Streamlit or a Python runtime.

## Current native modules

1. CSV data import with Chinese/English column aliases.
2. Built-in demo dataset.
3. Catalyst trajectory visualization drawn by a native Qt widget.
4. Per-catalyst initial/latest performance and retention.
5. Latest shared observed time and leader determination.
6. Censor-aware T95/T90/T80 endpoints, preserving the semantics used by the Python research engine: primary threshold lifetimes are not linearly interpolated into false exact values.
7. Experimental-condition comparability guard for temperature, GHSV, WHSV, pressure and feed ratio. Explicit mismatches suppress unsafe direct cross-catalyst leader claims.
8. Native pages for overview, data, lifetime analysis, AI migration workspace and settings.
9. Installer and portable Windows package workflow.

## Scientific guardrails already migrated

The C++ engine now preserves two important rules from the research implementation:

- Threshold lifetime claims remain censor-aware instead of inventing exact crossing times between sparse observations.
- Direct catalyst ranking is blocked when explicit experimental conditions are inconsistent between candidates.

The built-in `--self-test` verifies both a matched-condition demo dataset and a deliberately mismatched-temperature case.

## Planned migration

The Python implementation remains the scientific reference while modules are migrated. Next native modules are project persistence, richer condition-audit UI, Excel import, report export, document/evidence ingestion, external database clients, AI Analyst, Evidence Critic and audit logging.

## Build locally

```powershell
cmake -S native/qt -B build-qt -G "Visual Studio 17 2022" -A x64
cmake --build build-qt --config Release
```

Qt 6.5+ with the MSVC 2022 x64 kit must be available to CMake.

For a quick engine check:

```powershell
& "build-qt\Release\Catalyst Longevity Research.exe" --self-test
```
