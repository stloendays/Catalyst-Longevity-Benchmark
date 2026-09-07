"""Human-readable report rendering for Catalyst Longevity Analyzer."""
from __future__ import annotations

from typing import Any

from .friendly import build_user_summary


def render_markdown_report(report: dict[str, Any]) -> str:
    """Render a concise, user-facing Markdown report."""
    summary = build_user_summary(report)
    lines = [
        "# Catalyst Longevity Analysis Report",
        "",
        "## 一眼看懂",
        "",
        f"- 共分析 **{summary['catalyst_count']}** 个催化剂、**{summary['total_observations']}** 个时间点。",
        f"- 最长观测时间：**{summary['max_time_h']:g} h**。" if summary["max_time_h"] is not None else "- 最长观测时间：未提供。",
        f"- 在现有数据中发现 **{summary['observed_rank_changes']}** 组催化剂发生领先顺序变化。",
    ]
    if summary["latest_shared_leader"] is not None:
        lines.append(
            f"- 在最新共同观测时间 **{summary['latest_shared_time_h']:g} h**，"
            f"当前领先者为 **{summary['latest_shared_leader']}**。"
        )
    lines.append("")

    if summary["pairwise_conclusions"]:
        lines.extend(["## 催化剂之间的比较", ""])
        for text in summary["pairwise_conclusions"]:
            lines.append(f"- {text}")
        lines.append("")

    if summary["ranking_snapshots"]:
        lines.extend(["## 不同时间的排名", ""])
        for snapshot in summary["ranking_snapshots"]:
            lines.append(f"- {snapshot['time_h']:g} h：{snapshot['ranking_text']}")
        lines.append("")

    lines.extend(["## 各催化剂表现", ""])
    for card in summary["catalyst_cards"]:
        lines.append(f"### {card['catalyst_id']}")
        lines.append("")
        if card["initial_performance"] is not None:
            lines.append(f"- 初始性能：{card['initial_performance']:g}")
        if card["latest_performance"] is not None and card["latest_time_h"] is not None:
            lines.append(f"- {card['latest_time_h']:g} h 时性能：{card['latest_performance']:g}")
        if card["retention_percent"] is not None:
            lines.append(f"- 性能保持率：{card['retention_percent']:.1f}%")
        lines.append(f"- 95% 保持情况：{card['t95_text']}")
        lines.append(f"- 90% 保持情况：{card['t90_text']}")
        lines.append(f"- 80% 保持情况：{card['t80_text']}")
        lines.append("")

    lines.extend(
        [
            "## 如何理解结果",
            "",
            "- **性能保持率**表示最新观测值相对于该催化剂第一次观测值还剩多少。",
            "- **领先顺序变化**表示两个催化剂在不同实际观测时间点出现了前后名次交换。",
            "- **不同时间的排名**只使用该时间点实际存在的观测值，不对缺失时间点进行补值排名。",
            "- 当报告写着“测试结束时仍高于某阈值”时，意思是当前实验还没有测到真正的阈值寿命；不能把测试结束时间直接当成寿命。",
            "- 曲线之间的连线和累计面积只用于辅助观察，软件不会自动补造缺失实验点。",
            "",
            "## 数据与计算说明",
            "",
            "完整的原始计算状态、不确定性字段、来源信息和机器可读结果保存在 JSON 报告中。",
            "",
        ]
    )
    return "\n".join(lines)
