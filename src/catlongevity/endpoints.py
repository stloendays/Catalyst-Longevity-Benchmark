"""Utilities for censor-aware catalyst lifetime endpoints.

Primary threshold observations preserve the information content of discrete
TOS measurements. Interpolation is intentionally not performed here.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Optional


@dataclass(frozen=True)
class ThresholdObservation:
    threshold: float
    status: str
    lower_h: Optional[float]
    upper_h: Optional[float]


def threshold_observation(
    time_h: Iterable[float],
    activity: Iterable[float],
    threshold: float,
) -> ThresholdObservation:
    """Return exact/interval/right/left-censored first-passage information.

    Parameters
    ----------
    time_h:
        Monotonically increasing observation times.
    activity:
        Normalized activity values corresponding to ``time_h``.
    threshold:
        Activity threshold, e.g. 0.90 for t90.

    Notes
    -----
    The primary endpoint is based only on observed samples. A crossing between
    adjacent samples is interval-censored; no interpolation is imposed.
    """
    t = [float(x) for x in time_h]
    a = [float(x) for x in activity]
    if len(t) != len(a) or not t:
        raise ValueError("time_h and activity must have the same non-zero length")
    if any(t[i] > t[i + 1] for i in range(len(t) - 1)):
        raise ValueError("time_h must be monotonically increasing")
    if not 0 < threshold <= 1:
        raise ValueError("threshold must be in (0, 1]")

    # If the first usable sample is already below threshold, the crossing is
    # known only to have occurred at or before that observation.
    if a[0] < threshold:
        return ThresholdObservation(threshold, "left_censored", 0.0, t[0])
    if a[0] == threshold:
        return ThresholdObservation(threshold, "exact", t[0], t[0])

    for i in range(1, len(t)):
        if a[i] == threshold:
            return ThresholdObservation(threshold, "exact", t[i], t[i])
        if a[i] < threshold <= a[i - 1]:
            return ThresholdObservation(threshold, "interval", t[i - 1], t[i])

    return ThresholdObservation(threshold, "right_censored", t[-1], None)


def standard_thresholds(time_h: Iterable[float], activity: Iterable[float]):
    """Return censor-aware observations for t95, t90, and t80."""
    t = list(time_h)
    a = list(activity)
    return {
        "t95": threshold_observation(t, a, 0.95),
        "t90": threshold_observation(t, a, 0.90),
        "t80": threshold_observation(t, a, 0.80),
    }
