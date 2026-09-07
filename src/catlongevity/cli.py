"""Command-line interface for catalyst longevity analysis."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from .analysis import analyze_observations
from .io import read_tos_csv


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="catlongevity",
        description="Analyze catalyst time-on-stream trajectories with censor-aware lifetime and ranking logic.",
    )
    parser.add_argument("input_csv", help="Input CSV containing catalyst_id, time_h and performance columns")
    parser.add_argument("-o", "--output", default="catlongevity_report.json", help="Output JSON report path")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    observations = read_tos_csv(args.input_csv)
    report = analyze_observations(observations)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Catalyst Longevity report written to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
