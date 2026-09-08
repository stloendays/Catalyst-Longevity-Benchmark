#!/usr/bin/env python3
"""Validate synthetic Local2B MCP/tool-use datasets."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open(encoding="utf-8") as f:
        for lineno, line in enumerate(f, 1):
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise AssertionError(f"{path}:{lineno}: invalid JSON: {exc}") from exc
    return rows


def names_from_tools(row: dict[str, Any]) -> set[str]:
    return {t["function"]["name"] for t in row["tools"]}


def validate_row(row: dict[str, Any], seen_ids: set[str]) -> set[str]:
    rid = row["id"]
    assert rid not in seen_ids, f"duplicate id: {rid}"
    seen_ids.add(rid)

    assert row["representation"] == "qwen_function_call_messages"
    assert row["mcp_protocol_target"] == "2026-07-28"
    tools = names_from_tools(row)
    assert len(tools) >= 5, (rid, tools)

    calls: list[str] = []
    pending: list[str] = []
    for msg in row["messages"]:
        role = msg["role"]
        if role == "assistant":
            content = msg.get("content") or ""
            assert '"jsonrpc"' not in content, rid
            assert '"tools/call"' not in content, rid
            for call in msg.get("tool_calls") or []:
                fn = call["function"]
                name = fn["name"]
                assert name in tools, (rid, name, tools)
                assert isinstance(fn.get("arguments"), dict), (rid, fn)
                calls.append(name)
                pending.append(name)
        elif role == "tool":
            assert pending, f"{rid}: tool result without prior tool call"
            name = msg.get("name")
            assert name == pending.pop(0), (rid, name, pending)
            json.loads(msg["content"])

    assert not pending, f"{rid}: missing tool result(s): {pending}"
    assert calls == row["expected"]["tool_calls"], (rid, calls, row["expected"]["tool_calls"])
    return set(calls)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("data_dir", type=Path)
    args = ap.parse_args()

    manifest = json.loads((args.data_dir / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["model_target"] == "Qwen/Qwen3.5-2B"
    assert manifest["mcp_protocol_target"] == "2026-07-28"

    seen_ids: set[str] = set()
    called_by_split: dict[str, set[str]] = {}
    totals: dict[str, int] = {}

    for split in ("train", "val", "test"):
        rows = load_jsonl(args.data_dir / f"{split}.jsonl")
        called: set[str] = set()
        for row in rows:
            assert row["split"] == split
            called |= validate_row(row, seen_ids)
        called_by_split[split] = called
        totals[split] = len(rows)
        assert totals[split] == manifest["splits"][split]["records"]

    assert called_by_split["train"].isdisjoint(called_by_split["val"]), called_by_split
    assert called_by_split["train"].isdisjoint(called_by_split["test"]), called_by_split
    assert called_by_split["val"].isdisjoint(called_by_split["test"]), called_by_split

    print("DATASET_VALIDATION=PASS")
    print("records:", totals)
    print("called tools:", {k: sorted(v) for k, v in called_by_split.items()})


if __name__ == "__main__":
    main()
