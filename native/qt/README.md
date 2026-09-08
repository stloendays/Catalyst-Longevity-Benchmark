# Catalyst Longevity Research — Native Desktop

This directory contains the native Windows desktop migration of Catalyst Longevity Research.

## Technology

- C++20
- Qt 6 Widgets
- Qt SQL with SQLite project files
- Native PDF report generation through Qt
- Native `.xlsx` ingestion through pinned QXlsx
- CMake
- Native Windows executable deployment with `windeployqt`
- Inno Setup installer

The native application does not start a browser and does not require Streamlit or a Python runtime.

## Current native modules

1. Native project workspace with `.clrproj` SQLite save/open support.
2. Save / discard / cancel protection before closing, opening another project or starting a new project.
3. CSV data import with Chinese/English column aliases.
4. Native `.xlsx` workbook import. The importer scans workbook sheets and uses the first sheet whose header contains catalyst, time and performance columns.
5. Conservative text-document signal extraction for TXT / Markdown / CSV / TSV, including DOI, temperature, duration, CH4-conversion candidates, stability/deactivation keywords and source snippets.
6. Built-in demo dataset.
7. Catalyst trajectory visualization drawn by a native Qt widget.
8. Per-catalyst initial/latest performance and retention.
9. Latest shared observed time and leader determination.
10. Censor-aware T95/T90/T80 endpoints, preserving the semantics used by the Python research engine: primary threshold lifetimes are not linearly interpolated into false exact values.
11. Experimental-condition comparability guard for temperature, GHSV, WHSV, pressure and feed ratio. Explicit mismatches suppress unsafe direct cross-catalyst leader claims.
12. Condition-guard visualization showing audit status and the exact catalyst pairs/fields that block comparison.
13. Native PDF analysis report export including catalyst lifetime summaries, condition-audit status, mismatched pairs and guarded direct-comparison status.
14. Native pages for project management, overview, data, lifetime analysis, AI migration workspace and settings.
15. Installer and portable Windows package workflow.

## Excel import

The native build uses QXlsx as a statically linked build-time dependency. CMake pins QXlsx to commit `8a13e1c86e5d4fb5e3b2fb09c7b632514f1d54ca` (release v1.5.1.1) instead of tracking a moving branch. Set `-DCATALYST_FETCH_QXLSX=OFF` if a compatible system installation of `QXlsxQt6` is already available.

The same scientific column aliases used by CSV import are accepted for `.xlsx` files. Required concepts are catalyst, time and performance. Optional columns include temperature, GHSV, WHSV, pressure, feed ratio, metric and provenance/source.

## Document evidence backend

The C++ document analyzer mirrors the conservative signal-extraction policy of the Python research implementation. It treats isolated numbers as candidates rather than automatically converting them into experimental truth. Current native text ingestion supports TXT, Markdown, CSV and TSV. PDF text extraction is intentionally still on the migration roadmap rather than silently invoking OCR or introducing a weak parser.

The analyzer extracts:

- DOI candidates;
- explicit temperatures;
- explicit hour/minute durations normalized to hours;
- CH4/methane conversion percentage candidates;
- coking, sintering, stability/deactivation and regeneration terms;
- short source-locatable evidence snippets.

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

The built-in `--self-test` now verifies a matched-condition demo dataset, a deliberately mismatched-temperature case, SQLite save/load persistence, native PDF report creation, a generated `.xlsx` round trip, and conservative document-signal extraction.

## Planned migration

The Python implementation remains the scientific reference while modules are migrated. Next native modules are the document/evidence UI, native PDF text extraction, evidence persistence inside `.clrproj`, external database clients, AI Analyst, Evidence Critic and audit logging.

## Third-party notice

QXlsx is MIT licensed. The pinned revision and full license notice are recorded in `THIRD_PARTY_NOTICES.md` and should ship with the desktop package.

## Build locally

```powershell
cmake -S native/qt -B build-qt -G "Visual Studio 17 2022" -A x64
cmake --build build-qt --config Release
```

Qt 6.5+ with the MSVC 2022 x64 kit must be available to CMake. The default configure step also needs Git/network access once to fetch the pinned QXlsx revision unless `CATALYST_FETCH_QXLSX=OFF` is used with a preinstalled QXlsx package.

For a quick engine, persistence, report, Excel and evidence-backend check:

```powershell
& "build-qt\Release\Catalyst Longevity Research.exe" --self-test
```
