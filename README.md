# Catalyst Longevity Benchmark

**Catalyst Longevity Analyzer** turns catalyst time-series data into a simple answer: **which catalyst stays useful for longer, and does the best choice change over time?**

The repository still contains the underlying research benchmark and evidence registry, but the software interface is designed for users who simply want to upload data and get an interpretable result.

## What you can do

Upload a CSV or Excel table and quickly answer:

- Which catalyst starts with the highest performance?
- How much performance remains after 20, 50, 100 or more hours?
- Does an initially weaker catalyst later overtake the leader?
- Has the 95%, 90% or 80% performance-retention point actually been reached?
- Is the test long enough to make a lifetime claim?
- Which comparisons are clear, and which remain uncertain?

## Easiest way to use it

First install the browser interface dependencies:

```bash
python -m pip install -r requirements-ui.txt
```

Then launch the software with one command:

```bash
python launch.py
```

The browser interface will open automatically in most environments. You can then:

1. upload CSV / Excel data, or edit a table directly;
2. preview the data;
3. click **开始分析**;
4. inspect conclusions, curves and lifetime information;
5. download a spreadsheet summary, a simple report or full machine-readable output.

See [`docs/快速开始.md`](docs/快速开始.md) for a non-technical walkthrough.

## Your data can be very simple

Only three concepts are required:

| 催化剂 | 时间 | 性能 |
|---|---:|---:|
| Catalyst A | 0 | 82 |
| Catalyst A | 20 | 70 |
| Catalyst A | 50 | 58 |
| Catalyst B | 0 | 76 |
| Catalyst B | 20 | 72 |
| Catalyst B | 50 | 69 |

The importer accepts common English and Chinese headers, including:

- `catalyst_id`, `catalyst`, `sample`, `催化剂`, `样品`
- `time_h`, `time`, `TOS`, `时间`, `运行时间`
- `performance`, `value`, `conversion`, `activity`, `性能`, `转化率`, `活性`

Optional uncertainty and source columns can be added when available, but are not required for basic use.

A ready-to-edit template is available at [`examples/用户数据模板.csv`](examples/用户数据模板.csv), and the browser interface also includes a **下载数据模板** button.

## What the result looks like

The user-facing report focuses on plain conclusions:

```text
Catalyst A starts higher.
Catalyst B overtakes Catalyst A between 20 and 50 h.
At 50 h, Catalyst B retains 90.8% of its initial performance.
The 90% retention point for Catalyst B has not yet been reached.
```

The browser interface provides:

- headline statistics;
- interactive data preview and direct table editing;
- performance-over-time curves;
- catalyst-by-catalyst retention summaries;
- pairwise overtake conclusions;
- downloadable CSV summary, Markdown report and JSON data;
- an advanced-detail tab for users who want to inspect calculation semantics.

## Command-line option

For automated workflows:

```bash
python -m src.catlongevity.cli examples/用户数据模板.csv
```

This generates:

```text
catalyst_longevity_report.md
catalyst_longevity_report.json
```

## Software structure

```text
Catalyst-Longevity-Benchmark/
├── launch.py                      # one-command launcher
├── app.py                         # browser interface
├── requirements-ui.txt            # UI dependencies
├── examples/
│   ├── 用户数据模板.csv
│   └── example_tos_input.csv
├── docs/
│   ├── 快速开始.md
│   └── 软件操作说明.md
├── src/catlongevity/
│   ├── io.py                      # friendly import + validation
│   ├── endpoints.py               # lifetime threshold engine
│   ├── ranking.py                 # ranking and crossover engine
│   ├── analysis.py                # analysis orchestration
│   ├── friendly.py                # plain-language summaries
│   ├── reporting.py               # user-facing report
│   └── cli.py                     # command-line entry
├── data/                           # research/evidence data layer
├── protocols/                      # technical rules
├── reports/                        # research reports
└── tests/
```

## Built-in safeguards

The simple interface does not weaken the calculation rules underneath it.

- Missing experimental points are never fabricated.
- A test ending at 100 h is not automatically called a 100 h catalyst lifetime.
- Sparse points do not create a fake exact crossover time.
- Uncertain comparisons remain uncertain when the data do not support a firm order.
- Source data, digitized values and derived calculations remain distinguishable in the advanced layer.

In other words, the front end is simple, while the analysis engine remains conservative.

## Current application focus

The research dataset currently focuses on **Ni/Al2O3-based dry reforming of methane (DRM)**, but the software itself works with generic time-series performance data where higher values mean better performance.

This makes the same interface potentially useful for catalyst stability tests, material degradation studies, repeated performance measurements and other long-horizon comparison problems.
