# Pilot result: Zhou 2015 provides an independent full ranking reversal

## Source

Zhou L, Li L, Wei N, Li J, Basset J-M. *Effect of NiAl2O4 Formation on Ni/Al2O3 Stability during Dry Reforming of Methane*. ChemCatChem 2015, 7, 2508-2516. DOI: 10.1002/cctc.201500379.

Primary quantitative source: Table 3 in the KAUST repository manuscript.

Reaction conditions reported for Table 3: N2:CH4:CO2 = 2:1:1, F/W = 480 L gcat^-1 h^-1, 700 C.

## Source-supported observations

| Catalyst | TOF at 0.5 h (s^-1) | 15 h | 40 h | 100 h | Coke after 100 h (wt%) |
|---|---:|---:|---:|---:|---:|
| Ni/Al2O3(350) | 6.5 | 3.2 | 2.6 | 1.7 | 36.97 |
| Ni/Al2O3(700) | 5.9 | 3.6 | 3.2 | 2.9 | 16.33 |
| Ni/Al2O3(900) | 5.1 | 4.5 | 4.4 | 4.4 | 7.93 |

The first observed instantaneous rank is therefore

`350 > 700 > 900` at 0.5 h.

By 15 h it has fully reversed to

`900 > 700 > 350`,

and that ordering remains at 40 and 100 h.

This is an independent within-paper replication of the time-dependent ranking phenomenon already observed in the Zhang 2026 pilot. The primary claim does not require kinetic fitting or machine learning: each pairwise crossing is source-supported to occur within the interval `(0.5, 15] h`, assuming continuity of catalytic performance between observations.

## Sensitivity-only crossover estimates

If the sparse TOF observations are connected by straight lines, the instantaneous pairwise crossover estimates are approximately:

- 700 vs 900: 7.32 h
- 350 vs 900: 8.02 h
- 350 vs 700: 9.20 h

These values are **not observed crossing times** and must not replace the primary `(0.5, 15] h` brackets.

## Instantaneous ranking is not cumulative ranking

Using piecewise-linear trapezoidal integration beginning at the first observed point (0.5 h), the reconstructed cumulative TOF areas are:

| Horizon | 350 | 700 | 900 | cumulative rank |
|---|---:|---:|---:|---|
| 15 h | 70.325 | 68.875 | 69.600 | 350 > 900 > 700 |
| 40 h | 142.825 | 153.875 | 180.850 | 900 > 700 > 350 |
| 100 h | 271.825 | 336.875 | 444.850 | 900 > 700 > 350 |

Thus, by 15 h the **instantaneous** performance ranking has already fully reversed, while the **cumulative** objective still reflects the early high activity of the 350 C catalyst. Under the same piecewise-linear sensitivity reconstruction, the cumulative pairwise crossovers occur later than the instantaneous ones (roughly 14-19 h depending on the pair).

This motivates treating two decision objects separately:

1. `R_inst(H)`: instantaneous catalyst rank at operation time H.
2. `R_cum(H)`: rank by integrated production up to H.

They answer different engineering questions and need not change ordering at the same horizon.

## Mechanistic interpretation

The 900 C calcination condition produces NiAl2O4 before reduction and is reported to strengthen metal-support interaction. It sacrifices early TOF but strongly suppresses sintering/coking. Table 3 shows a monotonic reduction in 100 h coke amount from 36.97 to 16.33 to 7.93 wt% as the calcination condition moves from 350 to 700 to 900 C, coincident with much greater TOF retention.

This supports a stability-lever interpretation but does not, by itself, identify a universal causal law across papers. Cross-study mechanistic claims remain provisional until confounding and catalyst-family differences are addressed.

## Consequence for manuscript framing

The pilot evidence now supports a stronger but still provisional statement:

> Catalyst ranking in DRM is operation-horizon dependent, and the ranking obtained from instantaneous activity can differ from the ranking obtained from cumulative production.

The next requirement is external replication in additional independent Ni/Al2O3-family datasets and a censoring-aware meta-analysis that preserves different observation windows.