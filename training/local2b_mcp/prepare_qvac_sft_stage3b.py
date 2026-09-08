#!/usr/bin/env python3
"""Prepare the deterministic 64-example Step-3B server LoRA curriculum.

This wrapper reuses the frozen Step-3A QVAC conversion contract while selecting
a broader 64-example curriculum concentrated on the base model's measured weak
areas. Keeping this as a separate file avoids retriggering the Step-3A workflow
while that pilot is running.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

from prepare_qvac_sft import (
    FROZEN_TRAIN_SHA256,
    SEED,
    choose_rows,
    convert_record,
    load_jsonl,
    select_tools,
    sha256,
)

STAGE3B_QUOTAS: dict[str, int] = {
    "clarify": 10,
    "scope_guard": 10,
    "explicit_write": 10,
    "no_tool": 8,
    "search_then_read": 5,
    "zh_search_then_read": 5,
    "tool_error_recovery": 6,
    "stat_then_read": 4,
    "two_reads": 3,
    "tool_selection_collision": 3,
}


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--seed", type=int, default=SEED)
    p.add_argument("--max-tools", type=int, default=2)
    p.add_argument("--skip-hash-check", action="store_true")
    args = p.parse_args()

    source_hash = sha256(args.input)
    if not args.skip_hash_check and source_hash != FROZEN_TRAIN_SHA256:
        raise RuntimeError(f"frozen train hash mismatch: {source_hash} != {FROZEN_TRAIN_SHA256}")
    if args.max_tools < 1 or args.max_tools > 7:
        raise ValueError("--max-tools must be in [1, 7]")

    rows = load_jsonl(args.input)
    selected = choose_rows(rows, STAGE3B_QUOTAS, args.seed)
    converted = [convert_record(row, args.max_tools) for row in selected]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as f:
        for row in converted:
            f.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")

    task_counts = Counter(str(row.get("task_type", "unknown")) for row in selected)
    tool_counts = Counter(len(select_tools(row, args.max_tools)) for row in selected)
    char_lengths = [sum(len(m["content"]) for m in row["messages"]) for row in converted]
    manifest = {
        "stage": "3B",
        "preset": "cpu-stage3b-64",
        "seed": args.seed,
        "source": str(args.input),
        "source_sha256": source_hash,
        "output": str(args.output),
        "output_sha256": sha256(args.output),
        "records": len(converted),
        "task_counts": dict(sorted(task_counts.items())),
        "selected_tool_count_distribution": {str(k): v for k, v in sorted(tool_counts.items())},
        "max_tools": args.max_tools,
        "content_chars": {
            "min": min(char_lengths),
            "max": max(char_lengths),
            "mean": round(sum(char_lengths) / len(char_lengths), 1),
        },
        "format": "QVAC messages-only JSONL with Qwen3.5 XML tool calls and tool_response user turns",
        "assistant_only_loss": True,
        "intent": "broader hardware-constrained MCP specialization after Step-3A pilot",
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("QVAC_SFT_STAGE3B_PREP=PASS")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
