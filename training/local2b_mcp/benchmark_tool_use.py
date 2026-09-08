#!/usr/bin/env python3
"""Benchmark base/adapter model tool decisions through an OpenAI-compatible endpoint.

Each assistant decision is evaluated with gold prior context. This isolates model tool-use
quality from tool-runtime execution errors and gives a stable pre/post-LoRA benchmark.
"""

from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.request
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


def load_rows(path: Path, limit: int | None) -> list[dict[str, Any]]:
    rows = []
    with path.open(encoding="utf-8") as f:
        for line in f:
            rows.append(json.loads(line))
            if limit and len(rows) >= limit:
                break
    return rows


def norm_args(value: Any) -> dict[str, Any] | None:
    if isinstance(value, dict):
        return value
    if isinstance(value, str):
        try:
            parsed = json.loads(value)
            return parsed if isinstance(parsed, dict) else None
        except json.JSONDecodeError:
            return None
    return None


def canon_calls(calls: list[dict[str, Any]] | None) -> tuple[list[str], list[dict[str, Any] | None]]:
    names, args = [], []
    for call in calls or []:
        fn = call.get("function") or {}
        names.append(str(fn.get("name") or ""))
        args.append(norm_args(fn.get("arguments")))
    return names, args


def post_json(url: str, payload: dict[str, Any], timeout: int) -> dict[str, Any]:
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8"))


def convert_prefix(messages: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Convert gold training-format history to OpenAI API messages with tool_call_ids."""
    out: list[dict[str, Any]] = []
    pending_ids: list[tuple[str, str]] = []
    call_seq = 0
    for msg in messages:
        role = msg["role"]
        if role in {"system", "user"}:
            out.append({"role": role, "content": msg.get("content", "")})
        elif role == "assistant":
            calls = msg.get("tool_calls") or []
            if not calls:
                out.append({"role": "assistant", "content": msg.get("content", "")})
                continue
            api_calls = []
            for call in calls:
                call_seq += 1
                cid = f"gold_{call_seq}"
                fn = call["function"]
                api_calls.append({
                    "id": cid,
                    "type": "function",
                    "function": {"name": fn["name"], "arguments": json.dumps(fn["arguments"], ensure_ascii=False, separators=(",", ":"))},
                })
                pending_ids.append((fn["name"], cid))
            out.append({"role": "assistant", "content": msg.get("content") or None, "tool_calls": api_calls})
        elif role == "tool":
            name = msg.get("name")
            pos = next((i for i, (n, _) in enumerate(pending_ids) if n == name), None)
            if pos is None:
                raise ValueError(f"gold history tool result has no call: {name}")
            _, cid = pending_ids.pop(pos)
            out.append({"role": "tool", "tool_call_id": cid, "content": msg["content"]})
    return out


def decision_points(row: dict[str, Any]) -> list[tuple[list[dict[str, Any]], dict[str, Any]]]:
    """Return (gold prefix, target assistant) for tool decisions and the first tool-free answer."""
    points = []
    msgs = row["messages"]
    for i, msg in enumerate(msgs):
        if msg["role"] != "assistant":
            continue
        if msg.get("tool_calls"):
            points.append((msgs[:i], msg))
        elif i > 0 and msgs[i - 1]["role"] == "user":
            points.append((msgs[:i], msg))
    return points


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dataset", type=Path, required=True)
    ap.add_argument("--endpoint", default="http://127.0.0.1:18080/v1/chat/completions")
    ap.add_argument("--model", default="qwen3.5-2b-q4")
    ap.add_argument("--limit", type=int, default=160)
    ap.add_argument("--max-tokens", type=int, default=192)
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    rows = load_rows(args.dataset, args.limit)
    counts = Counter()
    by_cat: dict[str, Counter[str]] = defaultdict(Counter)
    failures = []
    latencies = []

    for row in rows:
        cat = row["task_type"]
        for point_idx, (prefix, target) in enumerate(decision_points(row)):
            counts["decisions"] += 1
            by_cat[cat]["decisions"] += 1
            expected_names, expected_args = canon_calls(target.get("tool_calls"))
            expected_tool = bool(expected_names)
            payload = {
                "model": args.model,
                "messages": convert_prefix(prefix),
                "tools": row["tools"],
                "tool_choice": "auto",
                "temperature": 0,
                "max_tokens": args.max_tokens,
            }
            t0 = time.perf_counter()
            try:
                resp = post_json(args.endpoint, payload, args.timeout)
                latencies.append(time.perf_counter() - t0)
                msg = resp["choices"][0]["message"]
                predicted_calls = msg.get("tool_calls") or []
                pred_names, pred_args = canon_calls(predicted_calls)
                predicted_tool = bool(pred_names)

                if expected_tool == predicted_tool:
                    counts["tool_gate_correct"] += 1
                    by_cat[cat]["tool_gate_correct"] += 1
                if not expected_tool and not predicted_tool:
                    counts["no_tool_correct"] += 1
                    by_cat[cat]["no_tool_correct"] += 1
                if expected_tool:
                    counts["tool_expected"] += 1
                    by_cat[cat]["tool_expected"] += 1
                    if pred_names == expected_names:
                        counts["tool_sequence_exact"] += 1
                        by_cat[cat]["tool_sequence_exact"] += 1
                    if len(pred_names) == len(expected_names) and all(x is not None for x in pred_args):
                        counts["valid_argument_shape"] += 1
                        by_cat[cat]["valid_argument_shape"] += 1
                    if pred_names == expected_names and pred_args == expected_args:
                        counts["call_exact"] += 1
                        by_cat[cat]["call_exact"] += 1
                else:
                    counts["no_tool_expected"] += 1
                    by_cat[cat]["no_tool_expected"] += 1
                    if predicted_tool:
                        counts["false_tool_call"] += 1
                        by_cat[cat]["false_tool_call"] += 1
            except (urllib.error.URLError, TimeoutError, KeyError, ValueError, json.JSONDecodeError) as exc:
                counts["api_or_parse_error"] += 1
                by_cat[cat]["api_or_parse_error"] += 1
                failures.append({"id": row["id"], "point": point_idx, "error": repr(exc)})

    d = counts["decisions"] or 1
    te = counts["tool_expected"] or 1
    ne = counts["no_tool_expected"] or 1
    report = {
        "benchmark": "local2b_mcp_step2_decision_benchmark_v1",
        "model": args.model,
        "endpoint": args.endpoint,
        "dataset": str(args.dataset),
        "records_loaded": len(rows),
        "counts": dict(counts),
        "metrics": {
            "tool_gate_accuracy": counts["tool_gate_correct"] / d,
            "tool_sequence_exact_accuracy": counts["tool_sequence_exact"] / te,
            "call_exact_accuracy": counts["call_exact"] / te,
            "valid_argument_shape_rate": counts["valid_argument_shape"] / te,
            "no_tool_precision": counts["no_tool_correct"] / ne,
            "false_tool_call_rate": counts["false_tool_call"] / ne,
            "api_or_parse_error_rate": counts["api_or_parse_error"] / d,
            "mean_latency_s": (sum(latencies) / len(latencies)) if latencies else None,
        },
        "by_task_type": {k: dict(v) for k, v in sorted(by_cat.items())},
        "failures": failures[:50],
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
