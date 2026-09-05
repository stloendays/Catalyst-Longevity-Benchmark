# Pilot crossover audit v0.1

## Purpose

The pilot audit is designed to test whether time-dependent catalyst-ranking changes can be recovered reproducibly from heterogeneous DRM literature without selecting only positive examples.

The audit therefore retains four evidence classes:

1. `confirmed_inversion` - numerical within-paper data prove that the ordering changes across observed TOS points.
2. `qualitative_activity_stability_tradeoff` - different activity and stability leaders are reported, but the numerical trajectory is insufficient to prove a later rank reversal.
3. `qualitative_concordant_activity_stability` - the same catalyst/condition is reported as both the activity and stability leader; these are important negative/comparator cases.
4. `insufficient_numeric_crossover_evidence` - stability evidence exists but the initial/later pairwise ordering cannot yet be established.

## Current directed pilot

As of 2026-09-06, the targeted pilot audit contains eight papers/records:

- confirmed numerical inversions: **2** (Zhou 2015; Zhang 2026)
- qualitative activity-stability tradeoff: **1** (Shen 2020)
- qualitative concordant activity/stability: **2** (Kwon 2022; Qiu 2022)
- insufficient numerical crossover evidence: **3** (Jin 2022; Shi 2022; Luo 2024)

## Critical caveat: this is NOT a prevalence estimate

The fraction `2/8` must **not** be reported as the prevalence or probability of catalyst-ranking inversion in DRM literature. The pilot set was deliberately assembled around mechanistically informative, stability-rich papers and is not a random or exhaustive sample. It is affected by targeted retrieval, publication practices, availability of long-term TOS data, and accessibility of numerical trajectories.

A defensible prevalence-style statement requires a prospectively defined literature registry with reproducible inclusion/exclusion rules and a denominator that includes eligible papers regardless of whether they support the hypothesis.

## Current evidence

### Confirmed inversion: Zhou 2015

Under matched conditions, Table 3 reports CH4 TOF for Ni/Al2O3 catalysts calcined at 350, 700 and 900 C. The first observed ranking at 0.5 h is `350 > 700 > 900`; by 15 h it is `900 > 700 > 350` and remains so at 40 and 100 h. All three pairwise inversions are bracketed in `(0.5, 15] h` without requiring kinetic fitting.

### Confirmed inversion: Zhang 2026

At matched 550 C conditions, R800 begins above R600 in CH4 conversion (20.4% vs 17.2%) but is below it after 20 h (12.6% vs 15.8%). The observed crossover is bracketed in `(0, 20] h`.

### Qualitative tradeoff: Shen 2020

The publisher abstract describes Ni/Al2O3-S as the activity leader and Ni/Al2O3-F as the stability leader with no sign of deactivation. Full numerical TOS data are required before this can be upgraded to a confirmed ranking inversion.

### Concordant comparators

Kwon 2022 reports the stoichiometric NiAl2O4-derived catalyst as both the most active and highly stable, with NiO-derived material showing the lowest activity and significant coking-driven deactivation. Qiu 2022 similarly points to a preparation/reduction condition that jointly improves performance and stability. These cases are retained explicitly to prevent confirmation bias.

## Next gate

The next scientific gate is not simply finding another positive example. It is:

- recover a third independent **numerical** crossover if it exists;
- simultaneously recover at least one high-quality **numerical non-inversion** comparator;
- then expand the denominator systematically before making any statement about frequency, predictors, or mechanism of ranking inversion.
