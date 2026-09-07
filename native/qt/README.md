# Catalyst Longevity Research — Native Desktop

This directory contains the native Windows desktop migration of Catalyst Longevity Research.

## Technology

- C++20
- Qt 6 Widgets
- Qt SQL with SQLite project files
- Native PDF report generation through Qt
- CMake
- Native Windows executable deployment with `windeployqt`
- Inno Setup installer

The native application does not start a browser and does not require Streamlit or a Python runtime.

## Current native modules

1. Native project workspace with `.clrproj` SQLite save/open support.
2. Save / discard / cancel protection before closing, opening another project or starting a new project.
3. CSV data import with Chinese/English column aliases.
4. Built-in demo dataset.
5. Catalyst trajectory visualization drawn by a native Qt widget.
6. Per-catalyst initial/latest performance and retention.
7. Latest shared observed time and leader determination.
8. Censor-aware T95/T90/T80 endpoints, preserving the semantics used by the Python research engine: primary threshold lifetimes are not linearly interpolated into false exact values.
9. Experimental-condition comparability guard for temperature, GHSV, WHSV, pressure and feed ratio. Explicit mismatches suppress unsafe direct cross-catalyst leader claims.
10. Condition-guard visualization showing audit status and the exact catalyst pairs/fields that block comparison.
11. Native PDF analysis report export including catalyst lifetime summaries, condition-audit status, mismatched pairs and guarded direct-comparison status.
12. Native pages for project management, overview, data, lifetime analysis, AI migration workspace and settings.
13. Installer and portable Windows package workflow.

## Project persistence

A `.clrproj` file is a SQLite database. The current schema stores:

- project metadata and schema version;
- catalyst identity;
- time-on-stream and performance;
- temperature, GHSV, WHSV and pressure when supplied;
- feed ratio, metric and provenance/source text.

The native `--self-test` performs a SQLite round trip using a temporary project file. This means the packaged build is expected to include and load the Qt SQLite driver, not merely compile against Qt SQL.

## Scientific guardrails already migrated

The C++ engine preserves two important rules from the research implementation:

- Threshold lifetime claims remain censor-aware instead of inventing exact crossing times between sparse observations.
- Direct catalyst ranking is blocked when explicit experimental conditions are inconsistent between candidates.

The built-in `--self-test` verifies a matched-condition demo dataset, a deliberately mismatched-temperature case, SQLite save/load persistence, and native PDF report creation.

## Planned migration

The Python implementation remains the scientific reference while modules are migrated. Next native modules are Excel import, document/evidence ingestion, external database clients, AI Analyst, Evidence Critic and audit logging.

## Build locally

```powershell
cmake -S native/qt -B build-qt -G "Visual Studio 17 2022" -A x64
cmake --build build-qt --config Release
```

Qt 6.5+ with the MSVC 2022 x64 kit must be available to CMake.

For a quick engine, persistence and report-export check:

```powershell
& "build-qt\Release\Catalyst Longevity Research.exe" --self-test
```
