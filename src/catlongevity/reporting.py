"""Human-readable report rendering for catalyst longevity analyses."""
from __future__ import annotations

from typing import Any


def _format_bound(value) -> str:
    return "unresolved" if value is None else f"{value:g} h"


def render_markdown_report(report: dict[str, Any]) -> str:
    """Render a concise auditable Markdown report from analysis output."""
    lines = [
        "# Catalyst Longevity Analysis Report",
        "",
        "## Analysis semantics",
        "",
        "- Threshold lifetimes preserve censoring status and do not interpolate primary crossing times.",
        "- Instantaneous crossover claims use common source-supported observation times.",
        "- Cumulative AUC values are piecewise-linear sensitivity quantities between observed points.",
        "- Uncertainty-aware reversal requires non-overlapping intervals to establish ordering.",
        "",
        "## Catalyst trajectories",
        "",
    ]

    for catalyst_id, catalyst in report["catalysts"].items():
        lines.extend(
            [
                f"### {catalyst_id}",
                "",
                f"- Observations: {catalyst['n_observations']}",
                f"- Paper IDs: {', '.join(catalyst['paper_ids']) if catalyst['paper_ids'] else 'not supplied'}",
                f"- Metrics: {', '.join(catalyst['metrics']) if catalyst['metrics'] else 'not supplied'}",
                f"- Provenance: {', '.join(catalyst['provenance_classes']) if catalyst['provenance_classes'] else 'not supplied'}",
            ]
        )
        for name in ("t95", "t90", "t80"):
            endpoint = catalyst["threshold_lifetimes"][name]
            lines.append(
                f"- {name}: {endpoint['status']} "
                f"[{_format_bound(endpoint['lower_h'])}, {_format_bound(endpoint['upper_h'])}]"
            )
        lines.extend(
            [
                "- Cumulative AUC: derived piecewise-linear sensitivity at observed times",
                "",
            ]
        )

    lines.extend(["## Pairwise ranking audit", ""])
    if not report["pairwise"]:
        lines.append("No pairwise comparison is available.")
    for pair in report["pairwise"]:
        lines.append(f"### {pair['catalyst_a']} vs {pair['catalyst_b']}")
        lines.append("")
        inst = pair["instantaneous_crossover"]
        if inst["status"] == "interval":
            lines.append(
                f"- Instantaneous crossover: interval-supported between "
                f"{inst['lower_h']:g} and {inst['upper_h']:g} h; direction = {inst['direction']}."
            )
        else:
            lines.append(f"- Instantaneous crossover: {inst['status']}.")

        uncertainty = pair["uncertainty_aware_crossover"]
        if uncertainty["status"] == "interval":
            lines.append(
                f"- Uncertainty-aware crossover: proven interval between "
                f"{uncertainty['lower_h']:g} and {uncertainty['upper_h']:g} h; "
                f"direction = {uncertainty['direction']}."
            )
        else:
            reason = uncertainty.get("reason")
            lines.append(
                f"- Uncertainty-aware crossover: {uncertainty['status']}"
                + (f" ({reason})." if reason else ".")
            )
        lines.append("")

    lines.extend(
        [
            "## Interpretation boundary",
            "",
            "This report separates source observations from derived sensitivity quantities. "
            "It does not infer missing curve points, does not convert sparse samples into exact crossing times, "
            "and does not assign catalyst-deactivation mechanisms without source evidence.",
            "",
        ]
    )
    return "\n".join(lines)
