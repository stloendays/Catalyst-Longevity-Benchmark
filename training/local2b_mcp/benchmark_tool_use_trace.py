#!/usr/bin/env python3
"""Trace tool-use decisions without changing the frozen benchmark implementation.

This diagnostic imports the frozen benchmark helpers and records every expected
and predicted decision, including tool names, arguments, assistant text and
finish reason. Metrics intentionally mirror benchmark_tool_use.py.
"""

from __future__ import annotations

import argparse
import json
import time
import urllib.error
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from benchmark_tool_use import canon_calls, convert_prefix, decision_points, load_rows, post_json


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dataset", type=Path, required=True)
    ap.add_argument("--endpoint", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--limit", type=int, default=32)
    ap.add_argument("--max-tokens", type=int, default=192)
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--trace-out", type=Path, required=True)
    args = ap.parse_args()

    rows = load_rows(args.dataset, args.limit)
    counts: Counter[str] = Counter()
    by_cat: dict[str, Counter[str]] = defaultdict(Counter)
    failures: list[dict[str, Any]] = []
    traces: list[dict[str, Any]] = []
    latencies: list[float] = []

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
            trace: dict[str, Any] = {
                "id": row["id"],
                "task_type": cat,
                "point": point_idx,
                "tool_inventory": [t.get("function", {}).get("name") for t in row.get("tools", [])],
                "expected_names": expected_names,
                "expected_args": expected_args,
                "expected_tool": expected_tool,
            }
            t0 = time.perf_counter()
            try:
                resp = post_json(args.endpoint, payload, args.timeout)
                latency = time.perf_counter() - t0
                latencies.append(latency)
                choice = resp["choices"][0]
                msg = choice["message"]
                predicted_calls = msg.get("tool_calls") or []
                pred_names, pred_args = canon_calls(predicted_calls)
                predicted_tool = bool(pred_names)
                gate_correct = expected_tool == predicted_tool
                sequence_exact = expected_tool and pred_names == expected_names
                valid_shape = expected_tool and len(pred_names) == len(expected_names) and all(x is not None for x in pred_args)
                call_exact = expected_tool and pred_names == expected_names and pred_args == expected_args

                if gate_correct:
                    counts["tool_gate_correct"] += 1
                    by_cat[cat]["tool_gate_correct"] += 1
                if not expected_tool and not predicted_tool:
                    counts["no_tool_correct"] += 1
                    by_cat[cat]["no_tool_correct"] += 1
                if expected_tool:
                    counts["tool_expected"] += 1
                    by_cat[cat]["tool_expected"] += 1
                    if sequence_exact:
                        counts["tool_sequence_exact"] += 1
                        by_cat[cat]["tool_sequence_exact"] += 1
                    if valid_shape:
                        counts["valid_argument_shape"] += 1
                        by_cat[cat]["valid_argument_shape"] += 1
                    if call_exact:
                        counts["call_exact"] += 1
                        by_cat[cat]["call_exact"] += 1
                else:
                    counts["no_tool_expected"] += 1
                    by_cat[cat]["no_tool_expected"] += 1
                    if predicted_tool:
                        counts["false_tool_call"] += 1
                        by_cat[cat]["false_tool_call"] += 1

                trace.update({
                    "predicted_names": pred_names,
                    "predicted_args": pred_args,
                    "predicted_content": msg.get("content"),
                    "predicted_tool": predicted_tool,
                    "finish_reason": choice.get("finish_reason"),
                    "gate_correct": gate_correct,
                    "sequence_exact": bool(sequence_exact),
                    "valid_argument_shape": bool(valid_shape),
                    "call_exact": bool(call_exact),
                    "latency_s": latency,
                })
            except (urllib.error.URLError, TimeoutError, KeyError, ValueError, json.JSONDecodeError) as exc:
                counts["api_or_parse_error"] += 1
                by_cat[cat]["api_or_parse_error"] += 1
                failure = {"id": row["id"], "point": point_idx, "error": repr(exc)}
                failures.append(failure)
                trace.update({"error": repr(exc), "latency_s": time.perf_counter() - t0})
            traces.append(trace)

    d = counts["decisions"] or 1
    te = counts["tool_expected"] or 1
    ne = counts["no_tool_expected"] or 1
    report = {
        "benchmark": "local2b_mcp_tool_trace_v1",
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
    args.trace_out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    with args.trace_out.open("w", encoding="utf-8") as f:
        for trace in traces:
            f.write(json.dumps(trace, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
