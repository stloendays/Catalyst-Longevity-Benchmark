"""Utilities for operation-horizon-dependent catalyst ranking.

Observed ranking reversals are separated from interpolation-based crossover
estimates. Instantaneous ranking and cumulative-performance ranking are also
kept distinct because they can reverse at different operation horizons.
Digitization/source uncertainty can be represented as value intervals so that
rank reversals are only called when the ordering is actually proven.
"""
from __future__ import annotations

from dataclasses import dataclass
from math import sqrt
from typing import Iterable, Optional


@dataclass(frozen=True)
class CrossoverBracket:
    status: str
    lower_h: Optional[float]
    upper_h: Optional[float]
    direction: Optional[str]


@dataclass(frozen=True)
class PerformanceInterval:
    """Closed uncertainty interval for one performance observation."""

    lower: float
    upper: float

    def __post_init__(self) -> None:
        if self.lower > self.upper:
            raise ValueError("performance interval lower bound must not exceed upper bound")


def _validated_pair(
    time_h: Iterable[float],
    performance_a: Iterable[float],
    performance_b: Iterable[float],
) -> tuple[list[float], list[float], list[float]]:
    t = [float(x) for x in time_h]
    a = [float(x) for x in performance_a]
    b = [float(x) for x in performance_b]
    if not t or len(t) != len(a) or len(t) != len(b):
        raise ValueError("time_h and both performance arrays must have the same non-zero length")
    if any(t[i] >= t[i + 1] for i in range(len(t) - 1)):
        raise ValueError("time_h must be strictly increasing")
    return t, a, b


def _validated_interval_pair(
    time_h: Iterable[float],
    performance_a: Iterable[PerformanceInterval | tuple[float, float]],
    performance_b: Iterable[PerformanceInterval | tuple[float, float]],
) -> tuple[list[float], list[PerformanceInterval], list[PerformanceInterval]]:
    t = [float(x) for x in time_h]

    def coerce(values: Iterable[PerformanceInterval | tuple[float, float]]) -> list[PerformanceInterval]:
        out: list[PerformanceInterval] = []
        for value in values:
            if isinstance(value, PerformanceInterval):
                out.append(value)
            else:
                lo, hi = value
                out.append(PerformanceInterval(float(lo), float(hi)))
        return out

    a = coerce(performance_a)
    b = coerce(performance_b)
    if not t or len(t) != len(a) or len(t) != len(b):
        raise ValueError("time_h and both performance-interval arrays must have the same non-zero length")
    if any(t[i] >= t[i + 1] for i in range(len(t) - 1)):
        raise ValueError("time_h must be strictly increasing")
    return t, a, b


def first_pairwise_crossover_bracket(
    time_h: Iterable[float],
    performance_a: Iterable[float],
    performance_b: Iterable[float],
) -> CrossoverBracket:
    """Return the first observed interval containing a pairwise rank reversal.

    A crossover is source-supported when the sign of A-B changes between two
    adjacent common observation times. No interpolation is required to claim
    that at least one crossing lies inside the interval, assuming performance
    changes continuously between observations.
    """
    t, a, b = _validated_pair(time_h, performance_a, performance_b)
    d = [x - y for x, y in zip(a, b)]
    for i in range(1, len(t)):
        if d[i - 1] > 0 and d[i] < 0:
            return CrossoverBracket("interval", t[i - 1], t[i], "A_to_B")
        if d[i - 1] < 0 and d[i] > 0:
            return CrossoverBracket("interval", t[i - 1], t[i], "B_to_A")
        if d[i] == 0 and d[i - 1] != 0:
            direction = "A_to_B" if d[i - 1] > 0 else "B_to_A"
            return CrossoverBracket("interval", t[i - 1], t[i], direction)

    return CrossoverBracket("not_observed", t[-1], None, None)


def first_proven_interval_crossover_bracket(
    time_h: Iterable[float],
    performance_a: Iterable[PerformanceInterval | tuple[float, float]],
    performance_b: Iterable[PerformanceInterval | tuple[float, float]],
) -> CrossoverBracket:
    """Return the first *proven* crossover bracket with interval-valued data.

    At a given observation time A is proven better than B only when A.lower is
    strictly greater than B.upper; B is proven better than A only when B.lower
    is strictly greater than A.upper. Overlapping intervals are treated as
    ambiguous and can never create a rank-reversal claim by themselves.

    If opposite proven orderings occur at two observation times, at least one
    crossover lies between those times under continuity. Ambiguous intermediate
    observations widen, rather than artificially sharpen, the bracket.

    If no opposite proven ordering is found, status is ``not_proven``. This is
    deliberately weaker than ``not_observed`` because overlapping uncertainty
    intervals may conceal a reversal.
    """
    t, a, b = _validated_interval_pair(time_h, performance_a, performance_b)

    def relation(x: PerformanceInterval, y: PerformanceInterval) -> Optional[str]:
        if x.lower > y.upper:
            return "A_over_B"
        if y.lower > x.upper:
            return "B_over_A"
        return None

    last_relation: Optional[str] = None
    last_relation_time: Optional[float] = None

    for ti, ai, bi in zip(t, a, b):
        current = relation(ai, bi)
        if current is None:
            continue
        if last_relation is not None and current != last_relation:
            direction = "A_to_B" if last_relation == "A_over_B" else "B_to_A"
            return CrossoverBracket("interval", last_relation_time, ti, direction)
        last_relation = current
        last_relation_time = ti

    return CrossoverBracket("not_proven", last_relation_time, t[-1], None)


def linear_crossover_estimate(
    t0: float,
    t1: float,
    a0: float,
    a1: float,
    b0: float,
    b1: float,
) -> float:
    """Estimate an instantaneous crossover by linear interpolation of A-B.

    This is a derived sensitivity quantity and must not replace the observed
    crossover bracket in primary reporting.
    """
    if t1 <= t0:
        raise ValueError("t1 must be greater than t0")
    d0 = float(a0) - float(b0)
    d1 = float(a1) - float(b1)
    if d0 == 0 or d1 == 0 or d0 * d1 > 0:
        raise ValueError("endpoint differences must have opposite signs and be non-zero")
    fraction = d0 / (d0 - d1)
    return float(t0) + fraction * (float(t1) - float(t0))


def cumulative_trapezoid_at_observed_times(
    time_h: Iterable[float],
    performance: Iterable[float],
) -> list[float]:
    """Return cumulative piecewise-linear AUC at each observed time.

    The first returned value is zero because integration begins at the first
    source-supported observation, not at an inferred t=0 value. Between source
    observations, linear interpolation is assumed. Therefore cumulative AUC is
    a derived sensitivity quantity unless the source itself supplies a dense
    or continuously recorded trajectory.
    """
    t = [float(x) for x in time_h]
    y = [float(x) for x in performance]
    if not t or len(t) != len(y):
        raise ValueError("time_h and performance must have the same non-zero length")
    if any(t[i] >= t[i + 1] for i in range(len(t) - 1)):
        raise ValueError("time_h must be strictly increasing")

    cumulative = [0.0]
    total = 0.0
    for i in range(1, len(t)):
        total += 0.5 * (y[i - 1] + y[i]) * (t[i] - t[i - 1])
        cumulative.append(total)
    return cumulative


def piecewise_linear_cumulative_crossover_estimate(
    time_h: Iterable[float],
    performance_a: Iterable[float],
    performance_b: Iterable[float],
) -> Optional[float]:
    """Estimate the first crossover of integrated A and B performance.

    Both trajectories are assumed linear between common observation times and
    integration starts at the first observation. The returned time is therefore
    a **sensitivity estimate**, not a source-observed crossover. The trivial
    equality of cumulative areas at the integration start is ignored.
    """
    t, a, b = _validated_pair(time_h, performance_a, performance_b)
    if len(t) < 2:
        return None

    d = [x - y for x, y in zip(a, b)]
    cumulative_diff = 0.0
    tol = 1e-12

    for i in range(len(t) - 1):
        dt = t[i + 1] - t[i]
        d0 = d[i]
        d1 = d[i + 1]
        slope = (d1 - d0) / dt

        # Within a segment, the cumulative AUC difference is
        # C(s) = C0 + d0*s + 0.5*slope*s^2, 0 <= s <= dt.
        roots: list[float] = []
        if abs(slope) <= tol:
            if abs(d0) > tol:
                roots.append(-cumulative_diff / d0)
        else:
            discriminant = d0 * d0 - 2.0 * slope * cumulative_diff
            if discriminant >= -tol:
                discriminant = max(0.0, discriminant)
                r = sqrt(discriminant)
                roots.extend(((-d0 - r) / slope, (-d0 + r) / slope))

        valid = sorted(s for s in roots if tol < s <= dt + tol)
        if valid:
            return t[i] + min(valid)

        cumulative_diff += 0.5 * (d0 + d1) * dt

    return None


def descending_rank(values: dict[str, float]) -> list[str]:
    """Return labels ordered from highest to lowest performance.

    Equal values are secondarily ordered by label for deterministic output.
    This helper does not assign statistical significance to small differences.
    """
    return [label for label, _ in sorted(values.items(), key=lambda kv: (-float(kv[1]), kv[0]))]
