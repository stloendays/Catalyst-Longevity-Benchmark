"""Input utilities for Catalyst Longevity Benchmark software workflows.

The input layer is intentionally conservative: it validates required fields,
keeps provenance metadata attached to each observation, and never invents
missing trajectory points.
"""
from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional


@dataclass(frozen=True)
class TOSObservation:
    catalyst_id: str
    time_h: float
    performance: float
    paper_id: Optional[str] = None
    metric: Optional[str] = None
    provenance_class: Optional[str] = None
    lower: Optional[float] = None
    upper: Optional[float] = None

    def __post_init__(self) -> None:
        if not self.catalyst_id.strip():
            raise ValueError("catalyst_id must be non-empty")
        if self.time_h < 0:
            raise ValueError("time_h must be non-negative")
        if self.lower is not None and self.upper is None:
            raise ValueError("uncertainty lower bound requires upper bound")
        if self.upper is not None and self.lower is None:
            raise ValueError("uncertainty upper bound requires lower bound")
        if self.lower is not None and self.upper is not None:
            if self.lower > self.upper:
                raise ValueError("uncertainty lower bound must not exceed upper bound")
            if not self.lower <= self.performance <= self.upper:
                raise ValueError("performance must lie inside its uncertainty interval")


def _optional_float(value: object) -> Optional[float]:
    text = "" if value is None else str(value).strip()
    return None if text == "" else float(text)


def read_tos_csv(path: str | Path) -> list[TOSObservation]:
    """Read a provenance-preserving time-on-stream CSV.

    Required columns are ``catalyst_id``, ``time_h`` and ``performance``.
    Optional columns are ``paper_id``, ``metric``, ``provenance_class``,
    ``lower`` and ``upper``.
    """
    input_path = Path(path)
    with input_path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        required = {"catalyst_id", "time_h", "performance"}
        missing = required.difference(reader.fieldnames or [])
        if missing:
            raise ValueError(f"missing required columns: {', '.join(sorted(missing))}")

        rows: list[TOSObservation] = []
        for line_number, row in enumerate(reader, start=2):
            try:
                rows.append(
                    TOSObservation(
                        catalyst_id=str(row["catalyst_id"]).strip(),
                        time_h=float(row["time_h"]),
                        performance=float(row["performance"]),
                        paper_id=(str(row.get("paper_id", "")).strip() or None),
                        metric=(str(row.get("metric", "")).strip() or None),
                        provenance_class=(str(row.get("provenance_class", "")).strip() or None),
                        lower=_optional_float(row.get("lower")),
                        upper=_optional_float(row.get("upper")),
                    )
                )
            except (TypeError, ValueError) as exc:
                raise ValueError(f"invalid input at CSV line {line_number}: {exc}") from exc

    if not rows:
        raise ValueError("input CSV contains no observations")
    return rows


def group_trajectories(observations: Iterable[TOSObservation]) -> dict[str, list[TOSObservation]]:
    """Group observations by catalyst and verify strictly increasing TOS."""
    grouped: dict[str, list[TOSObservation]] = {}
    for obs in observations:
        grouped.setdefault(obs.catalyst_id, []).append(obs)

    for catalyst_id, rows in grouped.items():
        rows.sort(key=lambda item: item.time_h)
        if any(rows[i].time_h >= rows[i + 1].time_h for i in range(len(rows) - 1)):
            raise ValueError(f"duplicate or non-increasing time_h for catalyst {catalyst_id}")
    return grouped
