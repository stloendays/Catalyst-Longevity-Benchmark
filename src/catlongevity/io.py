"""Input utilities for Catalyst Longevity Analyzer.

The user-facing layer accepts a small set of intuitive column aliases while the
analysis layer receives one canonical data model. Missing trajectory points are
never invented.
"""
from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Mapping, Optional


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
            raise ValueError("catalyst name cannot be empty")
        if self.time_h < 0:
            raise ValueError("time cannot be negative")
        if self.lower is not None and self.upper is None:
            raise ValueError("lower uncertainty bound requires an upper bound")
        if self.upper is not None and self.lower is None:
            raise ValueError("upper uncertainty bound requires a lower bound")
        if self.lower is not None and self.upper is not None:
            if self.lower > self.upper:
                raise ValueError("lower uncertainty bound cannot exceed upper bound")
            if not self.lower <= self.performance <= self.upper:
                raise ValueError("performance must lie inside its uncertainty interval")


COLUMN_ALIASES = {
    "catalyst_id": ("catalyst_id", "catalyst", "catalyst_name", "sample", "sample_name", "催化剂", "样品", "样品名称"),
    "time_h": ("time_h", "time", "tos_h", "tos", "hours", "hour", "时间", "时间_h", "运行时间"),
    "performance": ("performance", "value", "conversion", "activity", "tof", "性能", "转化率", "活性"),
    "paper_id": ("paper_id", "source_id", "来源", "文献编号"),
    "metric": ("metric", "measurement", "指标", "测量指标"),
    "provenance_class": ("provenance_class", "source_type", "数据来源", "来源类型"),
    "lower": ("lower", "lower_bound", "min", "下限"),
    "upper": ("upper", "upper_bound", "max", "上限"),
}


def _optional_float(value: object) -> Optional[float]:
    text = "" if value is None else str(value).strip()
    return None if text == "" else float(text)


def _canonical_column_map(fieldnames: Iterable[str]) -> dict[str, str]:
    """Map friendly/legacy column names to canonical internal names."""
    original = [str(name).strip() for name in fieldnames if name is not None]
    lowered = {name.casefold(): name for name in original}
    mapping: dict[str, str] = {}
    for canonical, aliases in COLUMN_ALIASES.items():
        for alias in aliases:
            if alias.casefold() in lowered:
                mapping[canonical] = lowered[alias.casefold()]
                break
    return mapping


def observations_from_records(records: Iterable[Mapping[str, object]]) -> list[TOSObservation]:
    """Convert table-like records into validated observations.

    Required concepts are catalyst name, time in hours, and performance. The
    input may use either canonical English headers or supported friendly aliases.
    """
    records = list(records)
    if not records:
        raise ValueError("no data rows were supplied")

    mapping = _canonical_column_map(records[0].keys())
    required = {"catalyst_id", "time_h", "performance"}
    missing = required.difference(mapping)
    if missing:
        friendly = {
            "catalyst_id": "catalyst / 催化剂",
            "time_h": "time_h / 时间",
            "performance": "performance / 转化率或活性",
        }
        names = ", ".join(friendly[name] for name in sorted(missing))
        raise ValueError(f"missing required columns: {names}")

    observations: list[TOSObservation] = []
    for row_number, row in enumerate(records, start=2):
        try:
            def get(canonical: str) -> object:
                source = mapping.get(canonical)
                return "" if source is None else row.get(source, "")

            observations.append(
                TOSObservation(
                    catalyst_id=str(get("catalyst_id")).strip(),
                    time_h=float(get("time_h")),
                    performance=float(get("performance")),
                    paper_id=(str(get("paper_id")).strip() or None),
                    metric=(str(get("metric")).strip() or None),
                    provenance_class=(str(get("provenance_class")).strip() or None),
                    lower=_optional_float(get("lower")),
                    upper=_optional_float(get("upper")),
                )
            )
        except (TypeError, ValueError) as exc:
            raise ValueError(f"problem in data row {row_number}: {exc}") from exc
    return observations


def read_tos_csv(path: str | Path) -> list[TOSObservation]:
    """Read a catalyst time-series CSV using friendly column aliases."""
    input_path = Path(path)
    if not input_path.exists():
        raise ValueError(f"input file not found: {input_path}")
    with input_path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames:
            raise ValueError("the CSV file has no header row")
        return observations_from_records(reader)


def group_trajectories(observations: Iterable[TOSObservation]) -> dict[str, list[TOSObservation]]:
    """Group observations by catalyst and verify increasing time values."""
    grouped: dict[str, list[TOSObservation]] = {}
    for obs in observations:
        grouped.setdefault(obs.catalyst_id, []).append(obs)

    for catalyst_id, rows in grouped.items():
        rows.sort(key=lambda item: item.time_h)
        if any(rows[i].time_h >= rows[i + 1].time_h for i in range(len(rows) - 1)):
            raise ValueError(
                f"{catalyst_id} contains duplicate or non-increasing time values; "
                "each catalyst needs one row per time point"
            )
    return grouped
