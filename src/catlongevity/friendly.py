"""User-facing summaries for Catalyst Longevity Analyzer.

The analysis engine keeps rigorous scientific semantics. This module translates
those results into plain-language descriptions for non-specialist users without
changing the underlying calculations.
"""
from __future__ import annotations

from typing import Any


STATUS_LABELS_ZH = {
    "exact": "已知准确时间",
    "interval": "发生在一个时间区间内",
    "right_censored": "测试结束时仍未达到该下降阈值",
    "left_censored": "第一次记录时已经达到该下降阈值",
    "unknown": "暂时无法判断",
    "not_observed": "在现有数据中未看到",
    "not_proven": "现有不确定性下无法确认",
    "not_evaluated": "缺少必要的不确定性信息",
    "not_evaluable": "可比较的数据点不足",
}


def describe_threshold(name: str, endpoint: dict[str, Any]) -> str:
    """Translate a threshold endpoint into plain Chinese."""
    status = endpoint.get("status", "unknown")
    lower = endpoint.get("lower_h")
    upper = endpoint.get("upper_h")
    threshold_pct = {"t95": 95, "t90": 90, "t80": 80}.get(name, int(endpoint.get("threshold", 0) * 100))

    if status == "exact" and lower is not None:
        return f"约在 {lower:g} 小时降到初始表现的 {threshold_pct}%"
    if status == "interval" and lower is not None and upper is not None:
        return f"在 {lower:g}–{upper:g} 小时之间降到初始表现的 {threshold_pct}%"
    if status == "right_censored" and lower is not None:
        return f"测试到 {lower:g} 小时时仍高于初始表现的 {threshold_pct}%"
    if status == "left_censored" and upper is not None:
        return f"第一次记录（{upper:g} 小时）时已经不高于初始表现的 {threshold_pct}%"
    return "现有数据不足以判断"


def describe_pairwise(pair: dict[str, Any]) -> str:
    """Return a short user-facing description of one pairwise comparison."""
    a = pair["catalyst_a"]
    b = pair["catalyst_b"]
    crossover = pair.get("instantaneous_crossover", {})
    status = crossover.get("status")

    if status == "interval":
        lower = crossover.get("lower_h")
        upper = crossover.get("upper_h")
        direction = crossover.get("direction")
        if direction == "A_to_B":
            return f"{a} 起初领先，但在 {lower:g}–{upper:g} 小时之间由 {b} 反超。"
        if direction == "B_to_A":
            return f"{b} 起初领先，但在 {lower:g}–{upper:g} 小时之间由 {a} 反超。"
        return f"{a} 与 {b} 在 {lower:g}–{upper:g} 小时之间发生领先顺序变化。"
    if status == "not_observed":
        return f"在共同观测时间内，没有看到 {a} 与 {b} 交换领先顺序。"
    if status == "not_evaluable":
        return f"{a} 与 {b} 的共同时间点不足，暂时无法判断是否发生反超。"
    return f"{a} 与 {b} 的领先顺序暂时无法给出明确结论。"


def build_ranking_snapshots(report: dict[str, Any]) -> list[dict[str, Any]]:
    """Rank catalysts at observed times shared by at least two catalysts."""
    catalysts = report.get("catalysts", {})
    by_time: dict[float, dict[str, float]] = {}
    for catalyst_id, item in catalysts.items():
        for time_h, performance in zip(item.get("time_h", []), item.get("performance", [])):
            by_time.setdefault(float(time_h), {})[catalyst_id] = float(performance)

    snapshots: list[dict[str, Any]] = []
    for time_h in sorted(by_time):
        values = by_time[time_h]
        if len(values) < 2:
            continue
        ranking = sorted(values, key=lambda name: (-values[name], name))
        snapshots.append(
            {
                "time_h": time_h,
                "leader": ranking[0],
                "ranking": ranking,
                "ranking_text": " > ".join(ranking),
                "values": {name: values[name] for name in ranking},
            }
        )
    return snapshots


def build_user_summary(report: dict[str, Any]) -> dict[str, Any]:
    """Build dashboard-friendly headline statistics and plain conclusions."""
    catalysts = report.get("catalysts", {})
    all_times = [t for item in catalysts.values() for t in item.get("time_h", [])]
    total_points = sum(item.get("n_observations", 0) for item in catalysts.values())

    catalyst_cards = []
    for catalyst_id, item in catalysts.items():
        performance = item.get("performance", [])
        time_h = item.get("time_h", [])
        initial = performance[0] if performance else None
        latest = performance[-1] if performance else None
        retention = None
        if initial not in (None, 0) and latest is not None:
            retention = 100.0 * latest / initial
        catalyst_cards.append(
            {
                "catalyst_id": catalyst_id,
                "initial_performance": initial,
                "latest_performance": latest,
                "latest_time_h": time_h[-1] if time_h else None,
                "retention_percent": retention,
                "t95_text": describe_threshold("t95", item["threshold_lifetimes"]["t95"]),
                "t90_text": describe_threshold("t90", item["threshold_lifetimes"]["t90"]),
                "t80_text": describe_threshold("t80", item["threshold_lifetimes"]["t80"]),
            }
        )

    pairwise_text = [describe_pairwise(pair) for pair in report.get("pairwise", [])]
    observed_crossovers = sum(
        1
        for pair in report.get("pairwise", [])
        if pair.get("instantaneous_crossover", {}).get("status") == "interval"
    )
    uncertainty_proven = sum(
        1
        for pair in report.get("pairwise", [])
        if pair.get("uncertainty_aware_crossover", {}).get("status") == "interval"
    )
    ranking_snapshots = build_ranking_snapshots(report)
    latest_shared = ranking_snapshots[-1] if ranking_snapshots else None

    return {
        "catalyst_count": len(catalysts),
        "total_observations": total_points,
        "max_time_h": max(all_times) if all_times else None,
        "observed_rank_changes": observed_crossovers,
        "uncertainty_proven_rank_changes": uncertainty_proven,
        "catalyst_cards": catalyst_cards,
        "pairwise_conclusions": pairwise_text,
        "ranking_snapshots": ranking_snapshots,
        "latest_shared_time_h": latest_shared["time_h"] if latest_shared else None,
        "latest_shared_leader": latest_shared["leader"] if latest_shared else None,
    }
