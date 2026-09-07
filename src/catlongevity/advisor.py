"""Decision-oriented recommendations built on top of conservative analysis results."""
from __future__ import annotations

from typing import Any


def _card_map(summary: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {card["catalyst_id"]: card for card in summary.get("catalyst_cards", [])}


def build_recommendations(report: dict[str, Any], summary: dict[str, Any]) -> list[dict[str, str]]:
    """Generate actionable Chinese recommendations without inventing evidence."""
    recommendations: list[dict[str, str]] = []
    cards = _card_map(summary)
    snapshots = summary.get("ranking_snapshots", [])
    condition_audit = report.get("condition_audit", {})

    if condition_audit.get("status") == "mismatch_detected":
        mismatches = condition_audit.get("pair_mismatches", {})
        examples = []
        for pair, fields in list(mismatches.items())[:4]:
            a, b = pair.split("||", 1)
            examples.append(f"{a} vs {b}: {', '.join(fields)}")
        recommendations.append(
            {
                "level": "数据质量",
                "title": "先解决实验条件不匹配，再做催化剂排名",
                "text": "系统检测到显式实验条件不一致，相关催化剂对的排名/反超结论已被禁用。" + (" 例：" + "；".join(examples) if examples else ""),
            }
        )
    elif condition_audit.get("status") == "conditions_not_provided":
        recommendations.append(
            {
                "level": "可比性",
                "title": "建议补充实验条件",
                "text": "当前没有温度、空速、压力或进料比字段。现有结果可以用于描述性比较，但关键选材前应确认不同催化剂是在可比条件下测试的。",
            }
        )

    if len(cards) == 1:
        recommendations.append(
            {
                "level": "建议",
                "title": "增加对照样品",
                "text": "当前只有一个催化剂轨迹。若目标是做材料选择，建议加入至少一个基准或候选催化剂，并尽量使用相同观测时间。",
            }
        )

    crossovers = [
        pair for pair in report.get("pairwise", [])
        if pair.get("instantaneous_crossover", {}).get("status") == "interval"
    ]
    if crossovers:
        for pair in crossovers[:5]:
            crossover = pair["instantaneous_crossover"]
            a, b = pair["catalyst_a"], pair["catalyst_b"]
            lo, hi = crossover.get("lower_h"), crossover.get("upper_h")
            recommendations.append(
                {
                    "level": "关键发现",
                    "title": f"{a} 与 {b} 的最佳选择随时间改变",
                    "text": f"现有观测支持两者在 {lo:g}–{hi:g} 小时之间发生领先顺序变化。实际选材时应先确定目标运行时长，而不是只看初始性能。",
                }
            )
            if lo is not None and hi is not None and hi > lo:
                mid = 0.5 * (lo + hi)
                recommendations.append(
                    {
                        "level": "下一步",
                        "title": "在反超区间增加测试点",
                        "text": f"如果要更准确定位 {a}/{b} 的选择边界，优先在约 {mid:g} 小时附近增加观测，并在 {lo:g}–{hi:g} 小时区间内加密测试。",
                    }
                )

    latest = snapshots[-1] if snapshots else None
    if latest:
        leader = latest["leader"]
        card = cards.get(leader, {})
        recommendations.append(
            {
                "level": "当前结论",
                "title": f"最新共同观测时间由 {leader} 领先",
                "text": f"在 {latest['time_h']:g} 小时的共同观测点，排名为 {latest['ranking_text']}。这个结论只针对该观测时间，不自动外推到更长运行周期。",
            }
        )
        retention = card.get("retention_percent")
        if retention is not None and retention >= 90:
            recommendations.append(
                {
                    "level": "建议",
                    "title": "现有测试可能仍不足以定义真实寿命",
                    "text": f"{leader} 在其最新记录时仍保留约 {retention:.1f}% 初始性能。若寿命是核心指标，应继续测试直至目标阈值被实际跨越，或明确按删失数据处理。",
                }
            )

    time_only_not_evaluable = [
        pair for pair in report.get("pairwise", [])
        if pair.get("instantaneous_crossover", {}).get("status") == "not_evaluable"
        and "condition" not in pair.get("instantaneous_crossover", {}).get("reason", "").lower()
    ]
    if time_only_not_evaluable:
        recommendations.append(
            {
                "level": "数据质量",
                "title": "统一测试时间点",
                "text": "部分催化剂缺少足够的共同观测时间，导致无法直接判断是否发生反超。建议后续实验使用统一的时间节点或至少保留关键共同时间点。",
            }
        )

    provenance_missing = []
    for catalyst_id, item in report.get("catalysts", {}).items():
        if not item.get("provenance_classes"):
            provenance_missing.append(catalyst_id)
    if provenance_missing:
        recommendations.append(
            {
                "level": "可追溯性",
                "title": "补充数据来源",
                "text": "以下样品尚未提供数据来源标签：" + "、".join(provenance_missing[:8]) + "。建议标记为实验原始值、文献表格、图像数字化或模型推导。",
            }
        )

    if not recommendations:
        recommendations.append(
            {
                "level": "结论",
                "title": "现有数据可完成基础长期表现比较",
                "text": "目前未发现需要特别警告的排名变化或可比性问题。若要用于关键选材决策，仍建议补充误差范围、重复实验和更长时间窗口。",
            }
        )
    return recommendations


def build_document_advice(signals: dict[str, Any]) -> list[str]:
    """Suggest next actions after a document has been parsed."""
    advice = []
    if signals.get("dois"):
        advice.append("已识别 DOI，可先通过 Crossref 校验论文身份、期刊、作者和年份。")
    else:
        advice.append("未自动识别 DOI；如果这是论文，建议手动提供 DOI，以便连接外部文献元数据。")
    if signals.get("durations_h"):
        advice.append("资料中识别到时间信息，可进一步定位稳定性测试区间并尝试形成 TOS 轨迹。")
    else:
        advice.append("尚未识别明确测试时长；建议确认图表横轴或正文中的 time-on-stream 描述。")
    if signals.get("ch4_conversion_percent_candidates"):
        advice.append("识别到 CH4 转化率候选值，但在进入自动排名前仍需绑定到具体催化剂、温度和时间点。")
    if signals.get("keyword_evidence", {}).get("coking"):
        advice.append("资料出现积碳相关证据词；机制标签只能在能定位到具体表征证据时写入。")
    if signals.get("keyword_evidence", {}).get("sintering"):
        advice.append("资料出现烧结相关证据词；建议进一步检查 TEM/XRD 等来源证据。")
    return advice
