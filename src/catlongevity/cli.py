"""Simple command-line interface for Catalyst Longevity Analyzer."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .analysis import analyze_observations
from .friendly import build_user_summary
from .io import read_tos_csv
from .reporting import render_markdown_report


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="catlongevity",
        description="Compare catalyst performance over time and generate an easy-to-read longevity report.",
    )
    parser.add_argument("input_csv", help="CSV file containing catalyst, time and performance columns")
    parser.add_argument("-o", "--output", default="catalyst_longevity_report.json", help="JSON output path")
    parser.add_argument("--markdown", default=None, help="Markdown report path")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        observations = read_tos_csv(args.input_csv)
        report = analyze_observations(observations)
        summary = build_user_summary(report)

        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

        markdown_path = Path(args.markdown) if args.markdown else output_path.with_suffix(".md")
        markdown_path.parent.mkdir(parents=True, exist_ok=True)
        markdown_path.write_text(render_markdown_report(report), encoding="utf-8")
    except Exception as exc:
        print(f"分析失败：{exc}", file=sys.stderr)
        print("提示：最少需要催化剂、时间、性能三列；同一催化剂不能出现重复时间点。", file=sys.stderr)
        return 2

    print("分析完成。")
    print(f"催化剂数量：{summary['catalyst_count']}")
    print(f"数据点：{summary['total_observations']}")
    if summary["max_time_h"] is not None:
        print(f"最长测试时间：{summary['max_time_h']:g} h")
    print(f"发现领先顺序变化：{summary['observed_rank_changes']} 组")
    for conclusion in summary["pairwise_conclusions"]:
        print(f"- {conclusion}")
    print(f"简明报告：{markdown_path}")
    print(f"完整数据：{output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
