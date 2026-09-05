# Catalyst Longevity Benchmark

A methods-first benchmark for studying heterogeneous catalyst deactivation, lifetime, and long-horizon performance without new experiments or large-scale computation.

## Scientific question

Does catalyst ranking based on initial activity remain valid after explicitly accounting for deactivation kinetics, finite time-on-stream observation windows, and censored lifetime measurements?

The v0.1 system is restricted to **Ni/Al2O3-based dry reforming of methane (DRM)** catalysts.

## Core objects

For a reported time-on-stream trajectory X(t), define normalized activity

`a(t) = X(t) / X0`.

Primary derived endpoints include:

- initial activity/conversion `X0`
- deactivation parameters (`kd`, `tau`, `beta`, model dependent)
- threshold lifetimes `t95`, `t90`, `t80`
- right-censoring indicators for unresolved lifetime thresholds
- horizon-specific integrated performance `AUC_H`
- operation-horizon-dependent catalyst ranking `Rank_i(H)`

## Frozen v0.1 rules

1. Primary system: Ni/Al2O3-based DRM.
2. Primary trajectory: CH4 conversion vs time-on-stream; CO2 conversion is secondary.
3. A test that ends before a lifetime threshold is crossed is treated as a right-censored observation, not a measured lifetime.
4. Primary validation is paper-grouped / leave-one-paper-out. Random-row splitting is not accepted as the main generalization estimate.
5. Mechanism labels such as coking or sintering require explicit source evidence.
6. Observational associations are not interpreted as causal effects without additional design/adjustment.

## Planned workflow

`Literature -> provenance-tracked TOS curves -> kinetic fitting -> censored lifetime endpoints -> dynamic ranking -> interpretable models -> mechanism-conditioned analysis`

## Repository layout

```text
.
├── README.md
├── data/
│   ├── registry/
│   ├── raw_digitized/
│   ├── interim/
│   └── processed/
├── schemas/
├── protocols/
├── src/catlongevity/
├── notebooks/
├── tests/
├── reports/
└── manuscript/
```

## Status

**v0.1 / Pilot literature audit.** Research protocol is frozen; literature screening and curve-level extraction are in progress.
