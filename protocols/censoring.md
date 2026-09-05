# Censoring and Lifetime Endpoint Protocol v0.2

## Principle

A finite time-on-stream (TOS) experiment usually does **not** observe a unique catalyst lifetime. It constrains the time at which a chosen activity threshold is crossed.

For normalized activity

`a(t) = X(t) / X0`, 

we define threshold lifetimes such as `t95`, `t90`, and `t80` as first-passage times below 0.95, 0.90, and 0.80 of the reference activity.

Primary data must preserve what the experiment actually identifies. Do not replace a bounded or censored observation with an interpolated or model-extrapolated point estimate.

## Observation taxonomy

Each threshold endpoint is stored as one of five observation types.

### 1. Exact

The source directly reports the threshold-crossing time, or an observation lies exactly on the threshold with sufficient time resolution.

Represent as `[L, U]` with `L = U = t_threshold`.

### 2. Interval-censored

The activity is above the threshold at one observed time and below it at the next observed time. The crossing is known only to lie between those observations.

Example:

- `a(10 h) = 0.93`
- `a(20 h) = 0.88`

Then `t90 in (10 h, 20 h]`.

For an endpoint-only report with `a(0) > 0.90` and `a(100 h) < 0.90`, the only source-supported statement may be `t90 in (0 h, 100 h]` until the full trajectory is recovered.

### 3. Right-censored

The experiment ends while activity remains above the threshold.

If `a(T_end) > 0.90`, then the observed statement is `t90 > T_end`.

Represent with `lower_h = T_end`, `upper_h = null`.

### 4. Left-censored

The first usable observation is already below the threshold and an earlier above-threshold point is unavailable.

Represent with `lower_h = 0` and `upper_h = t_first` (or another justified lower boundary).

### 5. Unknown

The source wording or units are insufficient to determine a defensible threshold observation.

Examples include ambiguous statements such as "decreased by 5%" when it is unclear whether this means a 5% relative decrease or 5 percentage points, and cases where the reported stability claim cannot be tied to a quantitative trajectory.

## Discrete TOS curves

For a digitized curve, the primary threshold observation is the interval between adjacent source-supported time points that bracket the threshold. Linear interpolation, splines, and kinetic-model crossing times are **derived estimates**, not primary observed lifetimes.

Store separately:

- observed censor type;
- observed lower and upper time bounds;
- any interpolated estimate and interpolation rule;
- any kinetic-model estimate and model family;
- fit diagnostics and uncertainty;
- extrapolation distance beyond the observed horizon.

## Reported deactivation constants

Author-reported deactivation rates/constants are preserved as reported quantities. Do not convert them to `t90` using an assumed exponential law unless the source explicitly defines the rate constant with that model or the conversion is clearly marked as a sensitivity analysis.

## Model-derived quantities

Candidate kinetic families include:

- Exponential: `a(t) = exp(-kd*t)`
- Power-law: `a(t) = (1 + kd*t)^(-n)`
- Weibull-like: `a(t) = exp(-(t/tau)^beta)`

No single family is assumed universal. Model extrapolations are sensitivity-analysis quantities and must not replace exact/interval/censored endpoints in the primary survival analysis.

## Horizon-integrated performance

For operation horizon `H`, define normalized integrated activity

`AUC_H = integral_0^H a(t) dt`.

Primary observed AUC should be limited to the observed TOS horizon unless an explicit integration/extrapolation model is declared. Rate-based cumulative productivity is preferred when cross-study rate data are genuinely comparable.

## Why this matters

Stability tests in the DRM literature have strongly heterogeneous observation windows. Treating all reported end-of-test stability values as equivalent lifetime measurements creates observation-window bias. The benchmark therefore preserves exact, interval-censored, right-censored, and left-censored information explicitly before any ranking or predictive modeling.