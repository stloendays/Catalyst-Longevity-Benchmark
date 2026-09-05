"""Utilities for operation-horizon-dependent catalyst ranking.

Observed ranking reversals are separated from interpolation-based crossover
estimates. Instantaneous ranking and cumulative-performance ranking are also
kept distinct because they can reverse at different operation horizons.
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
