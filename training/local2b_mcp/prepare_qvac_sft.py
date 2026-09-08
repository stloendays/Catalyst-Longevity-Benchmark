#!/usr/bin/env python3
"""Convert the frozen Step-2 tool-use curriculum into QVAC/llama.cpp SFT JSONL.

QVAC's assistant-only dataset reader consumes only ``messages`` and applies the
model's built-in chat template. It does not pass a top-level ``tools`` object to
the template. This converter therefore embeds a compact Qwen3.5-compatible tool
contract into the system turn, serializes assistant tool calls using Qwen3.5's
native XML call form, and maps tool results to ``<tool_response>`` user turns.

The CPU preset deliberately keeps the gold tools plus a small number of
confusable distractors. This reduces padded context cost on the 4-vCPU server
while preserving the decision boundaries Step 3 is meant to improve.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import random
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable

SEED = 20260908
FROZEN_TRAIN_SHA256 = "d2f3ec25be2599642f6355dcec997b55039137e60d9f0fb618ce18b0a7a42303"

CPU_STAGE3A_QUOTAS: dict[str, int] = {
    "clarify": 3,
    "scope_guard": 3,
    "explicit_write": 3,
    "no_tool": 2,
    "search_then_read": 1,
    "zh_search_then_read": 1,
    "tool_error_recovery": 1,
    "stat_then_read": 1,
    "two_reads": 1,
}

ROLE_PREFERENCES: dict[str, tuple[str, ...]] = {
    "clarify": ("read", "search", "stat"),
    "scope_guard": ("read", "search", "stat"),
    "explicit_write": ("write", "read", "stat"),
    "no_tool": ("read", "calc", "search"),
    "search_then_read": ("search", "read", "stat"),
    "zh_search_then_read": ("search", "read", "stat"),
    "tool_error_recovery": ("read", "search", "stat"),
    "stat_then_read": ("stat", "read", "search"),
    "two_reads": ("read", "search", "stat"),
    "tool_selection_collision": ("read", "search", "stat"),
}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_no}: invalid JSON") from exc
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: record must be an object")
            rows.append(row)
    return rows


def role_of_tool(tool: dict[str, Any]) -> str:
    fn = tool.get("function", {})
    desc = str(fn.get("description", "")).lower()
    if desc.startswith("read one"):
        return "read"
    if desc.startswith("list direct"):
        return "list"
    if desc.startswith("search indexed"):
        return "search"
    if desc.startswith("evaluate a basic arithmetic"):
        return "calc"
    if desc.startswith("retrieve one condition-reviewed"):
        return "evidence"
    if desc.startswith("write utf-8"):
        return "write"
    if desc.startswith("return metadata"):
        return "stat"
    return "unknown"


def called_tool_names(messages: Iterable[dict[str, Any]]) -> list[str]:
    names: list[str] = []
    for message in messages:
        for call in message.get("tool_calls", []) or []:
            fn = call.get("function", call)
            name = fn.get("name")
            if name and name not in names:
                names.append(str(name))
    return names


def select_tools(record: dict[str, Any], max_tools: int) -> list[dict[str, Any]]:
    tools = list(record.get("tools", []))
    if not tools:
        return []
    by_name = {str(t.get("function", {}).get("name")): t for t in tools}
    by_role: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for tool in tools:
        by_role[role_of_tool(tool)].append(tool)

    selected_names: list[str] = []
    for name in called_tool_names(record.get("messages", [])):
        if name in by_name and name not in selected_names:
            selected_names.append(name)

    task_type = str(record.get("task_type", ""))
    for role in ROLE_PREFERENCES.get(task_type, ()):
        for tool in by_role.get(role, []):
            name = str(tool.get("function", {}).get("name"))
            if name not in selected_names:
                selected_names.append(name)
            break
        if len(selected_names) >= max_tools:
            break

    for tool in tools:
        name = str(tool.get("function", {}).get("name"))
        if name not in selected_names:
            selected_names.append(name)
        if len(selected_names) >= max_tools:
            break

    selected = set(selected_names[:max_tools])
    return [tool for tool in tools if str(tool.get("function", {}).get("name")) in selected]


def compact_tool(tool: dict[str, Any]) -> dict[str, Any]:
    fn = tool.get("function", {})
    params = fn.get("parameters", {})
    props = params.get("properties", {})
    compact_props: dict[str, Any] = {}
    for name, spec in props.items():
        keep: dict[str, Any] = {}
        for key in ("type", "minimum", "maximum", "enum"):
            if key in spec:
                keep[key] = spec[key]
        compact_props[name] = keep
    return {
        "type": "function",
        "function": {
            "name": fn.get("name"),
            "description": fn.get("description", ""),
            "parameters": {
                "type": "object",
                "properties": compact_props,
                "required": list(params.get("required", [])),
                "additionalProperties": False,
            },
        },
    }


def tool_contract(tools: list[dict[str, Any]], original_system: str) -> str:
    tool_lines = "\n".join(
        json.dumps(compact_tool(tool), ensure_ascii=False, separators=(",", ":")) for tool in tools
    )
    return (
        "# Tools\n\n"
        "Available functions are listed as JSON schemas inside <tools>.\n"
        "<tools>\n"
        f"{tool_lines}\n"
        "</tools>\n\n"
        "For a function call, output only Qwen3.5 XML: <tool_call><function=NAME>"
        "<parameter=ARG>VALUE</parameter>...</function></tool_call>. "
        "Include every required parameter. If no function is needed, answer normally.\n\n"
        f"{original_system.strip()}"
    )


def format_arg(value: Any) -> str:
    if isinstance(value, bool):
        return "True" if value else "False"
    if isinstance(value, (dict, list)):
        return json.dumps(value, ensure_ascii=False, separators=(",", ":"))
    if value is None:
        return "None"
    return str(value)


def render_tool_calls(calls: Iterable[dict[str, Any]], prefix: str = "") -> str:
    blocks: list[str] = []
    for call in calls:
        fn = call.get("function", call)
        name = str(fn.get("name", ""))
        args = fn.get("arguments", {}) or {}
        if isinstance(args, str):
            args = json.loads(args)
        if not isinstance(args, dict):
            raise ValueError(f"tool arguments for {name} must be an object")
        lines = ["<tool_call>", f"<function={name}>"]
        for key, value in args.items():
            lines.extend([f"<parameter={key}>", format_arg(value), "</parameter>"])
        lines.extend(["</function>", "</tool_call>"])
        blocks.append("\n".join(lines))
    rendered = "\n".join(blocks)
    prefix = prefix.strip()
    return f"{prefix}\n\n{rendered}" if prefix and rendered else (prefix or rendered)


def convert_record(record: dict[str, Any], max_tools: int) -> dict[str, Any]:
    source_messages = list(record.get("messages", []))
    if not source_messages:
        raise ValueError("record has no messages")
    selected_tools = select_tools(record, max_tools=max_tools)
    selected_names = {str(t.get("function", {}).get("name")) for t in selected_tools}
    gold_names = set(called_tool_names(source_messages))
    if not gold_names.issubset(selected_names):
        missing = sorted(gold_names - selected_names)
        raise ValueError(f"selected tool set dropped gold tools: {missing}")

    original_system = ""
    if source_messages and source_messages[0].get("role") == "system":
        original_system = str(source_messages[0].get("content", ""))
        source_messages = source_messages[1:]
    messages: list[dict[str, str]] = [
        {"role": "system", "content": tool_contract(selected_tools, original_system)}
    ]

    i = 0
    while i < len(source_messages):
        msg = source_messages[i]
        role = str(msg.get("role", ""))
        if role == "tool":
            responses: list[str] = []
            while i < len(source_messages) and source_messages[i].get("role") == "tool":
                content = str(source_messages[i].get("content", ""))
                responses.append(f"<tool_response>\n{content}\n</tool_response>")
                i += 1
            messages.append({"role": "user", "content": "\n".join(responses)})
            continue
        if role == "assistant" and msg.get("tool_calls"):
            messages.append(
                {
                    "role": "assistant",
                    "content": render_tool_calls(msg.get("tool_calls", []), str(msg.get("content", ""))),
                }
            )
        elif role in {"system", "user", "assistant"}:
            messages.append({"role": role, "content": str(msg.get("content", ""))})
        else:
            raise ValueError(f"unsupported role: {role!r}")
        i += 1

    if any(m["role"] == "tool" for m in messages):
        raise AssertionError("QVAC output must not contain tool role messages")
    if not any(m["role"] == "assistant" and m["content"].strip() for m in messages):
        raise ValueError("record contains no non-empty assistant target")
    return {"messages": messages}


def choose_rows(rows: list[dict[str, Any]], quotas: dict[str, int], seed: int) -> list[dict[str, Any]]:
    buckets: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        buckets[str(row.get("task_type", "unknown"))].append(row)
    rng = random.Random(seed)
    chosen: list[dict[str, Any]] = []
    for task_type, quota in quotas.items():
        candidates = list(buckets.get(task_type, []))
        if len(candidates) < quota:
            raise ValueError(f"not enough {task_type} rows: {len(candidates)} < {quota}")
        rng.shuffle(candidates)
        chosen.extend(candidates[:quota])
    rng.shuffle(chosen)
    return chosen


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--preset", choices=["cpu-stage3a-16"], default="cpu-stage3a-16")
    p.add_argument("--seed", type=int, default=SEED)
    p.add_argument("--max-tools", type=int, default=3)
    p.add_argument("--skip-hash-check", action="store_true")
    args = p.parse_args()

    source_hash = sha256(args.input)
    if not args.skip_hash_check and source_hash != FROZEN_TRAIN_SHA256:
        raise RuntimeError(f"frozen train hash mismatch: {source_hash} != {FROZEN_TRAIN_SHA256}")
    if args.max_tools < 1 or args.max_tools > 7:
        raise ValueError("--max-tools must be in [1, 7]")

    rows = load_jsonl(args.input)
    quotas = CPU_STAGE3A_QUOTAS
    selected = choose_rows(rows, quotas, args.seed)
    converted = [convert_record(row, args.max_tools) for row in selected]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as f:
        for row in converted:
            f.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")

    task_counts = Counter(str(row.get("task_type", "unknown")) for row in selected)
    tool_counts = Counter(len(select_tools(row, args.max_tools)) for row in selected)
    char_lengths = [sum(len(m["content"]) for m in row["messages"]) for row in converted]
    manifest = {
        "stage": "3A",
        "preset": args.preset,
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
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("QVAC_SFT_PREP=PASS")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
