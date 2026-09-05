"""Utilities for operation-horizon-dependent catalyst ranking.

Observed ranking reversals are separated from interpolation-based crossover
estimates. The former are source-supported brackets; the latter are derived
sensitivity quantities.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Optional


@dataclass(frozen=True)
class CrossoverBracket:
    status: str
    lower_h: Optional[float]
    upper_h: Optional[float]
    direction: Optional[str]


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
    t = [float(x) for x in time_h]
    a = [float(x) for x in performance_a]
    b = [float(x) for x in performance_b]
    if not t or len(t) != len(a) or len(t) != len(b):
        raise ValueError("time_h and both performance arrays must have the same non-zero length")
    if any(t[i] >= t[i + 1] for i in range(len(t) - 1)):
        raise ValueError("time_h must be strictly increasing")

    d = [x - y for x, y in zip(a, b)]
    for i in range(1, len(t)):
        if d[i - 1] > 0 and d[i] < 0:
            return CrossoverBracket("interval", t[i - 1], t[i], "A_to_B")
        if d[i - 1] < 0 and d[i] > 0:
            return CrossoverBracket("interval", t[i - 1], t[i], "B_to_A")
        if d[i] == 0 and d[i - 1] != 0:
            # A sampled tie proves that a crossing occurred no later than this
            # sample, but does not prove the first crossing happened exactly at
            # the sample time.
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
    """Estimate a crossover time by linear interpolation of A-B.

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
