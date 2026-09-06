# Source-conflict protocol v0.1

A numerical disagreement between two source locations from the same paper is a provenance event, not a value to average away.

## Rules

1. Preserve every conflicting reported value together with its source locator and semantics (for example, `approximately` in an abstract versus a displayed table value in Supporting Information).
2. Do not average conflicting values and do not silently replace one source with another.
3. For the primary numeric registry, prefer the most specific source that directly represents the matched quantity and condition. A same-study table entry is preferred over a rounded narrative/abstract value when both purport to describe the same endpoint.
4. The non-primary value remains in `data/processed/source_value_conflicts_v0.1.csv` and in source-level notes.
5. If the discrepancy could alter a rank ordering, threshold-censor status, or manuscript claim, downgrade the derived result to unresolved until the conflict is reconciled from author data or an additional source.
6. Approximate narrative values may support qualitative ordering only when the ordering is robust to the discrepancy; they must not be used as exact kinetic-fitting targets.

## First registered case

Luo et al. 2024 (`10.1002/asia.202400700`) reports approximately 81.6% final CH4 conversion for Ni-0.16B/Al2O3 in the publisher abstract, while Supporting Information Table S4 reports 80.6% at 700 C, WHSV 18000 mL h^-1 g^-1, TOS 6000 min. The registry uses 80.6% as the exact displayed endpoint and preserves 81.6% as conflicting approximate source evidence.
