# Censoring and Lifetime Endpoint Protocol v0.1

## Principle

A finite stability test does not observe catalyst lifetime unless the chosen failure threshold is actually crossed.

For normalized activity

`a(t) = X(t) / X0`, 

we define threshold lifetimes such as `t95`, `t90`, and `t80`.

If a study ends at `T_end` with `a(T_end) > 0.90`, the correct observed statement is

`t90 > T_end`, 

which is a right-censored observation.

## Observed vs model-derived quantities

Store separately:

- observed threshold crossing time, when directly supported by the trajectory;
- censoring indicator;
- model-derived extrapolated threshold time;
- extrapolation distance beyond the observed horizon;
- model family and fit diagnostics.

Model extrapolations are sensitivity-analysis quantities and must not replace observed/censored endpoints in the primary survival analysis.

## Candidate kinetic families

- Exponential: `a(t) = exp(-kd*t)`
- Power-law: `a(t) = (1 + kd*t)^(-n)`
- Weibull-like: `a(t) = exp(-(t/tau)^beta)`

No single family is assumed universal. Model selection and diagnostics are performed within each trajectory and/or hierarchically, as appropriate.

## Horizon-integrated performance

For operation horizon `H`, define a normalized integrated activity

`AUC_H = integral_0^H a(t) dt`

or a rate-based cumulative productivity when comparable rate data are available.

Dynamic ranking is defined by comparing catalysts as a function of `H`, not by a single fixed endpoint.
