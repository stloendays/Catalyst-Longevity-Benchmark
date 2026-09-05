# Inclusion / Exclusion Protocol v0.1

## Scope

Primary benchmark: Ni/Al2O3-based heterogeneous catalysts for dry reforming of methane.

## Include

A study is eligible for the primary trajectory benchmark when:

1. The catalyst belongs to the Ni/Al2O3 family. Promoted, coated, morphologically modified, or NiAl2O4-derived variants may be retained if the alumina-supported Ni family remains the scientific comparison axis.
2. Dry reforming conditions are reported sufficiently to identify reaction temperature and feed composition; pressure and GHSV/WHSV are recorded where available.
3. A time-on-stream trajectory of CH4 conversion/activity can be recovered from a figure, table, or author-supplied dataset.
4. At least three time points can be recovered for primary kinetic fitting.
5. Catalyst formulation can be uniquely or near-uniquely identified.

## Secondary-only

A record may be retained for secondary analyses when:

- only start/end values are recoverable;
- the stability test is quantitative but too short or too sparse for robust kinetic fitting;
- the support is a scope-edge alumina-derived material (for example, Mg-Al mixed oxide) useful for sensitivity analysis but not the strict core benchmark.

## Exclude

Exclude from the primary benchmark when:

- the TOS axis or activity/conversion definition is ambiguous;
- the figure resolution or curve overlap prevents defensible extraction;
- regeneration, feed switching, or temperature-program changes occur and cannot be segmented;
- only qualitative claims such as "stable for 100 h" are provided without a recoverable quantitative trajectory;
- the record duplicates data from an original paper; the original source is the primary provenance object.

## Mechanism labeling

Mechanism labels are evidence-constrained:

- `coking`
- `sintering`
- `mixed`
- `other`
- `unknown`

A non-unknown label requires explicit source evidence, e.g. TGA/TPO, TEM, XRD, Raman, XANES, DRIFTS, or another defensible characterization method. Author interpretation and our statistical inference are stored separately.

## Data leakage rule

Catalysts from the same paper must remain in the same validation fold. Primary predictive evaluation uses paper-grouped or leave-one-paper-out validation; random-row splitting is prohibited as the headline validation result.
