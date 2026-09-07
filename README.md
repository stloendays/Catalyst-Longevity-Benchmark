# Catalyst Longevity Benchmark

A methods-first research software system for catalyst deactivation, censored lifetime analysis, and operation-horizon-dependent catalyst ranking.

## Scientific question

Does catalyst ranking based on initial activity remain valid after explicitly accounting for deactivation kinetics, finite time-on-stream observation windows, censored lifetime measurements, and cumulative long-horizon performance?

The current scientific scope is restricted to **Ni/Al2O3-based dry reforming of methane (DRM)** catalysts, while the analysis software is designed around generic time-on-stream trajectories.

## What the software does

```text
TOS CSV input
  -> validated provenance-preserving trajectories
  -> normalized activity
  -> censor-aware t95 / t90 / t80
  -> instantaneous ranking and crossover audit
  -> uncertainty-aware ranking audit
  -> cumulative AUC sensitivity
  -> JSON + Markdown report
```

Run the included synthetic example from the repository root:

```bash
python -m src.catlongevity.cli examples/example_tos_input.csv
```

The software writes:

```text
catlongevity_report.json
catlongevity_report.md
```

See `docs/软件操作说明.md` for the formal input specification, workflow, output definitions, error handling, and scientific interpretation boundaries.

## Core scientific objects

For a reported time-on-stream trajectory X(t), normalized activity is defined as:

`a(t) = X(t) / X(first observed time)`

Primary outputs include:

- observed initial activity/conversion or TOF
- threshold lifetimes `t95`, `t90`, `t80`
- exact / interval / right-censored / left-censored lifetime status
- instantaneous ranking `R_inst(H)`
- pairwise crossover brackets at common observed times
- uncertainty-aware proven/non-proven ordering
- cumulative performance `AUC_H`
- cumulative-production ranking `R_cum(H)` when the comparison is scientifically supported
- provenance and source-conflict metadata

## Frozen scientific rules

1. Primary system: Ni/Al2O3-based DRM.
2. Primary trajectory: CH4 conversion vs time-on-stream; CO2 conversion is secondary.
3. A test that ends before a lifetime threshold is crossed is right-censored, not a measured lifetime.
4. `exact` lifetime is reserved for directly source-reported crossing times; sparse sampled trajectories do not create exact crossings.
5. Instantaneous ranking and cumulative-production ranking are separate scientific quantities.
6. Piecewise-linear interpolation and trapezoidal AUC from sparse literature points are **derived sensitivity quantities**, not source-observed truth.
7. Ranking reversal under interval-valued uncertainty requires non-overlapping performance intervals to establish opposite orderings.
8. Primary cross-paper validation is paper-grouped / leave-one-paper-out; random-row splitting is not the main generalization estimate.
9. Mechanism labels such as coking or sintering require explicit source evidence.
10. Source-observed, digitized and model-derived values remain distinct.
11. Conflicting source values are preserved rather than silently averaged.
12. Observational associations are not interpreted as causal effects without additional design or adjustment.

## Repository layout

```text
.
├── README.md
├── data/
│   ├── registry/
│   ├── raw_digitized/
│   ├── interim/
│   └── processed/
├── docs/
│   └── 软件操作说明.md
├── examples/
│   └── example_tos_input.csv
├── protocols/
├── reports/
├── schemas/
├── src/catlongevity/
│   ├── io.py
│   ├── endpoints.py
│   ├── ranking.py
│   ├── analysis.py
│   ├── reporting.py
│   └── cli.py
└── tests/
```

## Evidence status

The repository contains confirmed within-paper examples demonstrating that catalyst ordering can change with operation horizon, together with numerical concordant controls where the observed leader remains the leader. Qualitative activity-stability tradeoffs are not promoted to confirmed ranking reversals without numerical end-rank evidence.

## Development principle

The software is developed under a fail-closed scientific-data policy: missing points are not invented, uncertain ordering is not upgraded to proven ordering, sparse trajectories are not assigned exact crossover times, and synthetic demonstration data are never mixed into the evidence registry.
