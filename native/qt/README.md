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
5. Native `资料分析` workspace for TXT / Markdown / CSV / TSV evidence files.
6. Conservative document signal extraction: DOI, temperature, duration, CH4-conversion candidates, stability/deactivation keywords and source snippets.
7. Evidence-candidate binding to a selected catalyst and optional observation time.
8. Evidence persistence inside `.clrproj`, including source path/hash, candidate value, excerpt, binding state and user note.
9. Built-in demo dataset.
10. Catalyst trajectory visualization drawn by a native Qt widget.
11. Per-catalyst initial/latest performance and retention.
12. Latest shared observed time and leader determination.
13. Censor-aware T95/T90/T80 endpoints. Primary threshold lifetimes are not linearly interpolated into false exact values.
14. Experimental-condition comparability guard for temperature, GHSV, WHSV, pressure and feed ratio. Explicit mismatches suppress unsafe direct cross-catalyst leader claims.
15. Condition-guard visualization showing the exact catalyst pairs/fields that block comparison.
16. Native PDF analysis report export.
17. Installer and portable Windows package workflow.

## Excel import

The native build uses QXlsx as a statically linked build-time dependency. CMake pins QXlsx to commit `8a13e1c86e5d4fb5e3b2fb09c7b632514f1d54ca` (release v1.5.1.1) instead of tracking a moving branch. Set `-DCATALYST_FETCH_QXLSX=OFF` if a compatible system installation of `QXlsxQt6` is already available.

The same scientific column aliases used by CSV import are accepted for `.xlsx` files. Required concepts are catalyst, time and performance. Optional columns include temperature, GHSV, WHSV, pressure, feed ratio, metric and provenance/source.

## Document evidence workspace

The C++ document analyzer mirrors the conservative signal-extraction policy of the Python research implementation. It treats isolated numbers as candidates rather than automatically converting them into experimental truth. The native `资料分析` page currently supports TXT, Markdown, CSV and TSV.

Each extracted candidate is assigned a SHA-256 source identity and can be bound to a catalyst and optional time-on-stream value. Binding is deliberately not equivalent to scientific acceptance. The binding states remain explicit:

- `candidate_requires_condition_binding`
- `bound_to_catalyst_requires_time_condition_review`
- `bound_to_catalyst_time_requires_condition_review`

These evidence items are stored separately from experimental observations. They do not silently alter performance trajectories or rankings.

PDF text extraction remains intentionally on the migration roadmap rather than silently invoking OCR or introducing a weak parser. Scanned PDF content will not be promoted into structured evidence without an explicit, auditable extraction path.

## Project persistence

A `.clrproj` file is a SQLite database. The schema stores experimental observations and, additively, an `evidence_items` table. The evidence table was added without changing the project schema version, so earlier schema-version-1 projects remain readable and simply contain zero evidence items until resaved.

The current project file preserves:

- catalyst identity, time-on-stream and performance;
- temperature, GHSV, WHSV and pressure when supplied;
- feed ratio, metric and provenance/source text;
- evidence source path and SHA-256;
- evidence category, term/value and source excerpt;
- bound catalyst, optional bound time, review status and user note.

## Scientific guardrails already migrated

The C++ engine preserves the core research rules:

- Threshold lifetime claims remain censor-aware instead of inventing exact crossing times between sparse observations.
- Direct catalyst ranking is blocked when explicit experimental conditions are inconsistent between candidates.
- Extracted literature/document values remain evidence candidates until explicitly bound and reviewed.
- Even catalyst/time-bound document evidence is not automatically injected into the experimental ranking engine.

The built-in `--self-test` verifies matched-condition analysis, a deliberately mismatched-temperature case, SQLite save/load, PDF report creation, generated `.xlsx` round trip, conservative document extraction, evidence candidate generation, evidence binding and evidence persistence round trip.

## Validated Windows packaging

The Windows workflow uses the MSVC 2022 Qt kit on `windows-2022`, builds the native GUI, runs the self-test, smoke-tests the real desktop window, deploys Qt, re-runs the application with the Qt SDK removed from `PATH`, builds an Inno Setup installer, creates a portable ZIP and publishes a prerelease.

## Planned migration

The Python implementation remains the scientific reference while modules are migrated. Next native modules are explicit condition-review approval for bound evidence, native PDF text extraction, evidence-aware PDF reporting, external database clients, AI Analyst, Evidence Critic and audit logging.

## Third-party notice

QXlsx is MIT licensed. The pinned revision and full license notice are recorded in `THIRD_PARTY_NOTICES.md` and ship with the desktop package.

## Build locally

```powershell
cmake -S native/qt -B build-qt -G "Visual Studio 17 2022" -A x64
cmake --build build-qt --config Release
```

Qt 6.5+ with the MSVC 2022 x64 kit must be available to CMake. The default configure step also needs Git/network access once to fetch the pinned QXlsx revision unless `CATALYST_FETCH_QXLSX=OFF` is used with a preinstalled QXlsx package.

For the native engine/persistence/report/Excel/evidence check:

```powershell
& "build-qt\Release\Catalyst Longevity Research.exe" --self-test
```
