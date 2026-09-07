"""High-level analysis orchestration for catalyst longevity trajectories.

This module converts validated observations into a reproducible report while
preserving the distinction between source-supported quantities and derived
sensitivity quantities.
"""
from __future__ import annotations

from itertools import combinations
from typing import Any

from .endpoints import standard_thresholds
from .io import TOSObservation, group_trajectories
from .ranking import (
    PerformanceInterval,
    cumulative_trapezoid_at_observed_times,
    first_pairwise_crossover_bracket,
    first_proven_interval_crossover_bracket,
)


def _threshold_to_dict(observation) -> dict[str, Any]:
    return {
        "threshold": observation.threshold,
        "status": observation.status,
        "lower_h": observation.lower_h,
        "upper_h": observation.upper_h,
    }


def _normalize(rows: list[TOSObservation]) -> list[float]:
    baseline = rows[0].performance
    if baseline == 0:
        raise ValueError(f"cannot normalize catalyst {rows[0].catalyst_id}: first performance is zero")
    return [row.performance / baseline for row in rows]


def analyze_observations(observations: list[TOSObservation]) -> dict[str, Any]:
    """Create a machine-readable catalyst longevity report.

    Primary threshold lifetimes remain exact/interval/left/right-censored as
    defined in ``endpoints.py``. Cumulative AUC is explicitly labeled as a
    piecewise-linear sensitivity quantity.
    """
    grouped = group_trajectories(observations)
    report: dict[str, Any] = {
        "software": "Catalyst Longevity Benchmark",
        "analysis_semantics": {
            "threshold_lifetimes": "censor-aware; no interpolation used for primary crossing status",
            "instantaneous_crossover": "source-supported only at common observed times",
            "cumulative_auc": "piecewise-linear sensitivity between observed points",
            "uncertainty_crossover": "requires non-overlapping performance intervals to prove ordering",
        },
        "catalysts": {},
        "pairwise": [],
    }

    for catalyst_id, rows in sorted(grouped.items()):
        time_h = [row.time_h for row in rows]
        performance = [row.performance for row in rows]
        normalized = _normalize(rows)
        thresholds = standard_thresholds(time_h, normalized)
        cumulative_auc = cumulative_trapezoid_at_observed_times(time_h, performance)

        report["catalysts"][catalyst_id] = {
            "paper_ids": sorted({row.paper_id for row in rows if row.paper_id}),
            "metrics": sorted({row.metric for row in rows if row.metric}),
            "provenance_classes": sorted({row.provenance_class for row in rows if row.provenance_class}),
            "n_observations": len(rows),
            "time_h": time_h,
            "performance": performance,
            "normalized_activity": normalized,
            "threshold_lifetimes": {name: _threshold_to_dict(obs) for name, obs in thresholds.items()},
            "cumulative_auc_at_observed_times": cumulative_auc,
            "cumulative_auc_semantics": "derived_piecewise_linear_sensitivity",
        }

    for catalyst_a, catalyst_b in combinations(sorted(grouped), 2):
        rows_a = grouped[catalyst_a]
        rows_b = grouped[catalyst_b]
        by_time_a = {row.time_h: row for row in rows_a}
        by_time_b = {row.time_h: row for row in rows_b}
        common_times = sorted(set(by_time_a).intersection(by_time_b))

        pair: dict[str, Any] = {
            "catalyst_a": catalyst_a,
            "catalyst_b": catalyst_b,
            "common_observation_times_h": common_times,
        }
        if len(common_times) >= 2:
            perf_a = [by_time_a[t].performance for t in common_times]
            perf_b = [by_time_b[t].performance for t in common_times]
            crossover = first_pairwise_crossover_bracket(common_times, perf_a, perf_b)
            pair["instantaneous_crossover"] = {
                "status": crossover.status,
                "lower_h": crossover.lower_h,
                "upper_h": crossover.upper_h,
                "direction": crossover.direction,
            }

            have_intervals = all(
                by_time_a[t].lower is not None
                and by_time_a[t].upper is not None
                and by_time_b[t].lower is not None
                and by_time_b[t].upper is not None
                for t in common_times
            )
            if have_intervals:
                intervals_a = [PerformanceInterval(by_time_a[t].lower, by_time_a[t].upper) for t in common_times]
                intervals_b = [PerformanceInterval(by_time_b[t].lower, by_time_b[t].upper) for t in common_times]
                proven = first_proven_interval_crossover_bracket(common_times, intervals_a, intervals_b)
                pair["uncertainty_aware_crossover"] = {
                    "status": proven.status,
                    "lower_h": proven.lower_h,
                    "upper_h": proven.upper_h,
                    "direction": proven.direction,
                }
            else:
                pair["uncertainty_aware_crossover"] = {
                    "status": "not_evaluated",
                    "reason": "complete uncertainty intervals are not available at all common observation times",
                }
        else:
            pair["instantaneous_crossover"] = {
                "status": "not_evaluable",
                "reason": "at least two common observation times are required",
            }
            pair["uncertainty_aware_crossover"] = {
                "status": "not_evaluable",
                "reason": "at least two common observation times are required",
            }
        report["pairwise"].append(pair)

    return report
