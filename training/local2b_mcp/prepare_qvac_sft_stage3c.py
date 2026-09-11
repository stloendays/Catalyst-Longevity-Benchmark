#!/usr/bin/env python3
"""Prepare a fresh balanced Stage-3C rehearsal curriculum.

Stage 3C is a recovery design after Stage 3B showed catastrophic held-out tool-use
regression. It is intentionally prepared from the frozen Step-2 train split, not
from an existing adapter. Every selected record keeps the full seven-tool
inventory so training matches the held-out inference environment. The 48-record
mix rehearses all 16 task families while modestly up-weighting the four primary
base weaknesses: clarification, scope guarding, no-tool decisions, and exact
writes.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

from prepare_qvac_sft import (
    FROZEN_TRAIN_SHA256,
    SEED,
    choose_rows,
    load_jsonl,
    render_tool_calls,
    sha256,
)

STAGE3C_QUOTAS: dict[str, int] = {
    "single_read": 2,
    "search_then_read": 3,
    "calculator": 2,
    "directory_list": 2,
    "evidence_lookup": 2,
    "explicit_write": 5,
    "stat_then_read": 3,
    "no_tool": 5,
    "clarify": 5,
    "scope_guard": 5,
    "tool_error_recovery": 3,
    "tool_selection_collision": 2,
    "optional_search_limit": 2,
    "two_reads": 2,
    "zh_single_read": 2,
    "zh_search_then_read": 3,
}


def full_tool_contract(tools: list[dict[str, Any]], original_system: str) -> str:
    """Use full schemas and a Qwen3.5-compatible tool contract.

    Unlike Stage 3A/3B, no parameter descriptions or schema fields are removed.
    This narrows the training/inference distribution gap without copying runtime
    transport details into the learned target.
    """
    tool_lines = "\n".join(
        json.dumps(tool, ensure_ascii=False, separators=(",", ":")) for tool in tools
    )
    return (
        "# Tools\n\n"
        "You have access to the following function schemas.\n\n"
        "<tools>\n"
        f"{tool_lines}\n"
        "</tools>\n\n"
        "When a function is needed, emit the Qwen3.5 <tool_call> XML form, include every required "
        "parameter, and do not append text after the call. If no function is needed, answer normally. "
        "Choose by description and schema rather than by memorizing tool names.\n\n"
        f"{original_system.strip()}"
    )


def convert_record_full(record: dict[str, Any]) -> dict[str, Any]:
    source_messages = list(record.get("messages", []))
    if not source_messages:
        raise ValueError("record has no messages")
    tools = list(record.get("tools", []))
    if len(tools) != 7:
        raise ValueError(f"Stage 3C requires exactly seven tools, got {len(tools)}")

    inventory = {str(t.get("function", {}).get("name")) for t in tools}
    called: set[str] = set()
    for message in source_messages:
        for call in message.get("tool_calls", []) or []:
            fn = call.get("function", call)
            if fn.get("name"):
                called.add(str(fn["name"]))
    if not called.issubset(inventory):
        raise ValueError(f"gold tool missing from full inventory: {sorted(called - inventory)}")

    original_system = ""
    if source_messages and source_messages[0].get("role") == "system":
        original_system = str(source_messages[0].get("content", ""))
        source_messages = source_messages[1:]

    messages: list[dict[str, str]] = [
        {"role": "system", "content": full_tool_contract(tools, original_system)}
    ]
    i = 0
    while i < len(source_messages):
        msg = source_messages[i]
        role = str(msg.get("role", ""))
        if role == "tool":
            responses: list[str] = []
            while i < len(source_messages) and source_messages[i].get("role") == "tool":
                responses.append(
                    "<tool_response>\n"
                    + str(source_messages[i].get("content", ""))
                    + "\n</tool_response>"
                )
                i += 1
            messages.append({"role": "user", "content": "\n".join(responses)})
            continue
        if role == "assistant" and msg.get("tool_calls"):
            messages.append(
                {
                    "role": "assistant",
                    "content": render_tool_calls(
                        msg.get("tool_calls", []), str(msg.get("content", ""))
                    ),
                }
            )
        elif role in {"system", "user", "assistant"}:
            messages.append({"role": role, "content": str(msg.get("content", ""))})
        else:
            raise ValueError(f"unsupported role: {role!r}")
        i += 1

    if not any(m["role"] == "assistant" and m["content"].strip() for m in messages):
        raise ValueError("record contains no non-empty assistant target")
    return {"messages": messages}


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--seed", type=int, default=SEED)
    p.add_argument("--skip-hash-check", action="store_true")
    args = p.parse_args()

    source_hash = sha256(args.input)
    if not args.skip_hash_check and source_hash != FROZEN_TRAIN_SHA256:
        raise RuntimeError(f"frozen train hash mismatch: {source_hash} != {FROZEN_TRAIN_SHA256}")

    rows = load_jsonl(args.input)
    selected = choose_rows(rows, STAGE3C_QUOTAS, args.seed)
    converted = [convert_record_full(row) for row in selected]
    if len(converted) != 48:
        raise AssertionError(f"expected 48 Stage-3C records, got {len(converted)}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as f:
        for row in converted:
            f.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")

    task_counts = Counter(str(row.get("task_type", "unknown")) for row in selected)
    char_lengths = [sum(len(m["content"]) for m in row["messages"]) for row in converted]
    manifest = {
        "stage": "3C",
        "preset": "cpu-stage3c-balanced-full7-48",
        "seed": args.seed,
        "source": str(args.input),
        "source_sha256": source_hash,
        "output": str(args.output),
        "output_sha256": sha256(args.output),
        "records": len(converted),
        "task_counts": dict(sorted(task_counts.items())),
        "selected_tool_count_distribution": {"7": len(converted)},
        "content_chars": {
            "min": min(char_lengths),
            "max": max(char_lengths),
            "mean": round(sum(char_lengths) / len(char_lengths), 1),
        },
        "format": "QVAC messages-only JSONL; full seven-tool schemas; Qwen3.5 XML calls; tool_response user turns",
        "assistant_only_loss": True,
        "training_initialization": "fresh_from_base_no_lora_init",
        "design_reason": "balanced rehearsal to prevent catastrophic forgetting and remove 2-tool training / 7-tool inference mismatch",
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("QVAC_SFT_STAGE3C_PREP=PASS")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
