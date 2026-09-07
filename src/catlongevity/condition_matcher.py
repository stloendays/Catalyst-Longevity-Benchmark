"""Experimental-condition matching for safe catalyst comparisons."""
from __future__ import annotations

from copy import deepcopy
from typing import Any, Iterable, Mapping


CONDITION_ALIASES = {
    "temperature_c": ("temperature_c", "temperature", "temp_c", "temp", "温度", "反应温度"),
    "ghsv": ("ghsv", "GHSV", "气时空速"),
    "whsv": ("whsv", "WHSV", "质量空速"),
    "pressure_bar": ("pressure_bar", "pressure", "压力", "反应压力"),
    "feed_ratio": ("feed_ratio", "ch4_co2_ratio", "CH4:CO2", "进料比", "气体比例"),
    "reaction": ("reaction", "reaction_name", "反应", "反应类型"),
}

NUMERIC_FIELDS = {"temperature_c", "ghsv", "whsv", "pressure_bar"}
CATALYST_ALIASES = ("catalyst_id", "catalyst", "catalyst_name", "sample", "sample_name", "催化剂", "样品", "样品名称")


def _resolve_column(keys: Iterable[str], aliases: Iterable[str]) -> str | None:
    lowered = {str(key).strip().casefold(): str(key) for key in keys}
    for alias in aliases:
        match = lowered.get(alias.casefold())
        if match is not None:
            return match
    return None


def _clean(value: object) -> str | None:
    if value is None:
        return None
    text = str(value).strip()
    if not text or text.casefold() in {"nan", "none", "null"}:
        return None
    return text


def _normalize_condition(field: str, value: object) -> str | None:
    text = _clean(value)
    if text is None:
        return None
    if field in NUMERIC_FIELDS:
        normalized = text.replace(",", "").replace("°C", "").replace("℃", "").strip()
        try:
            number = float(normalized)
        except ValueError:
            return text.casefold()
        return f"{number:g}"
    if field == "feed_ratio":
        return text.replace(" ", "").replace("：", ":").casefold()
    return " ".join(text.split()).casefold()


def audit_conditions(records: Iterable[Mapping[str, object]]) -> dict[str, Any]:
    """Audit whether explicit experimental conditions support pairwise comparison."""
    records = list(records)
    if not records:
        return {"status": "no_data", "explicit_fields": [], "catalysts": {}, "pair_mismatches": {}}

    keys = list(records[0].keys())
    catalyst_col = _resolve_column(keys, CATALYST_ALIASES)
    if catalyst_col is None:
        return {"status": "no_catalyst_column", "explicit_fields": [], "catalysts": {}, "pair_mismatches": {}}

    columns = {
        canonical: _resolve_column(keys, aliases)
        for canonical, aliases in CONDITION_ALIASES.items()
    }
    columns = {key: value for key, value in columns.items() if value is not None}
    if not columns:
        return {
            "status": "conditions_not_provided",
            "explicit_fields": [],
            "catalysts": {},
            "pair_mismatches": {},
            "message": "未提供显式实验条件；可以做描述性比较，但关键选材结论建议补充温度、空速、压力和进料条件。",
        }

    values_by_catalyst: dict[str, dict[str, set[str]]] = {}
    for row in records:
        catalyst = _clean(row.get(catalyst_col))
        if not catalyst:
            continue
        entry = values_by_catalyst.setdefault(catalyst, {field: set() for field in columns})
        for field, source_col in columns.items():
            value = _normalize_condition(field, row.get(source_col))
            if value is not None:
                entry[field].add(value)

    catalyst_summary: dict[str, dict[str, Any]] = {}
    for catalyst, fields in values_by_catalyst.items():
        catalyst_summary[catalyst] = {
            field: sorted(values) for field, values in fields.items() if values
        }

    names = sorted(catalyst_summary)
    pair_mismatches: dict[str, list[str]] = {}
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            mismatch_fields = []
            for field in columns:
                a_values = catalyst_summary[a].get(field, [])
                b_values = catalyst_summary[b].get(field, [])
                if len(a_values) == 1 and len(b_values) == 1 and a_values[0] != b_values[0]:
                    mismatch_fields.append(field)
                elif len(a_values) > 1 or len(b_values) > 1:
                    mismatch_fields.append(field)
            if mismatch_fields:
                pair_mismatches[f"{a}||{b}"] = mismatch_fields

    status = "mismatch_detected" if pair_mismatches else "matched_on_provided_conditions"
    return {
        "status": status,
        "explicit_fields": sorted(columns),
        "source_columns": columns,
        "catalysts": catalyst_summary,
        "pair_mismatches": pair_mismatches,
        "message": (
            "发现显式实验条件不匹配，相关催化剂对不会输出直接排名或反超结论。"
            if pair_mismatches
            else "在用户提供的实验条件字段上未发现催化剂之间的不匹配。"
        ),
    }


def apply_condition_guard(report: dict[str, Any], audit: dict[str, Any]) -> dict[str, Any]:
    """Disable pairwise ranking claims for catalyst pairs with explicit mismatches."""
    guarded = deepcopy(report)
    guarded["condition_audit"] = audit
    mismatch_map = audit.get("pair_mismatches", {})
    if not mismatch_map:
        return guarded

    for pair in guarded.get("pairwise", []):
        a, b = pair["catalyst_a"], pair["catalyst_b"]
        fields = mismatch_map.get(f"{a}||{b}") or mismatch_map.get(f"{b}||{a}")
        if not fields:
            continue
        pair["condition_mismatch_fields"] = fields
        pair["instantaneous_crossover"] = {
            "status": "not_evaluable",
            "reason": "explicit experimental-condition mismatch: " + ", ".join(fields),
        }
        pair["uncertainty_aware_crossover"] = {
            "status": "not_evaluable",
            "reason": "explicit experimental-condition mismatch: " + ", ".join(fields),
        }
    return guarded
