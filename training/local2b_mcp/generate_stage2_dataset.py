#!/usr/bin/env python3
"""Generate the expanded Step-2 MCP/tool-use curriculum for Qwen3.5-2B."""

from __future__ import annotations

import argparse
import hashlib
import json
import random
from collections import Counter
from pathlib import Path
from typing import Any

SYSTEM = (
    "You are a compact local work agent. Choose tools from their descriptions and JSON schemas, "
    "not from memorized names. Use tools for workspace state, exact arithmetic, evidence retrieval, "
    "or explicit writes. Never invent tool results. Stay inside declared scope. Ask one concise "
    "clarification when a required target is ambiguous."
)

ALIAS_POOLS = {
    "train": {
        "read": ["workspace_read_text", "project_file_open", "text_asset_load", "repo_read_utf8"],
        "list": ["workspace_list_entries", "project_dir_scan", "repo_folder_list", "directory_inventory"],
        "search": ["workspace_text_search", "project_corpus_find", "repo_query_text", "indexed_text_lookup"],
        "calc": ["exact_arithmetic", "numeric_expression_eval", "calculator_compute", "math_eval_exact"],
        "evidence": ["reviewed_record_get", "evidence_record_fetch", "source_record_lookup", "reviewed_evidence_read"],
        "write": ["workspace_write_text", "project_file_save", "repo_text_write", "text_asset_store"],
        "stat": ["workspace_file_stat", "project_path_info", "repo_file_metadata", "path_status_get"],
    },
    "val": {
        "read": ["document_readback", "workspace_document_get"],
        "list": ["folder_contents_get", "workspace_folder_scan"],
        "search": ["document_corpus_search", "workspace_semantic_find"],
        "calc": ["precise_number_eval", "expression_solver"],
        "evidence": ["reviewed_source_get", "evidence_item_retrieve"],
        "write": ["document_text_save", "workspace_document_put"],
        "stat": ["document_metadata_get", "workspace_path_probe"],
    },
    "test": {
        "read": ["artifact_text_fetch", "project_document_pull"],
        "list": ["artifact_folder_enumerate", "project_directory_walk"],
        "search": ["artifact_index_query", "project_knowledge_search"],
        "calc": ["deterministic_math_compute", "project_numeric_calculate"],
        "evidence": ["validated_source_retrieve", "project_evidence_pull"],
        "write": ["artifact_text_commit", "project_document_save"],
        "stat": ["artifact_metadata_probe", "project_path_inspect"],
    },
}

FILES = {
    "reports/run_summary.md": "Run 42 passed. Fe ranked first after economic propagation.",
    "notes/meeting.txt": "Decision: keep the local model for low-risk text tasks only.",
    "data/metrics.csv": "metric,value\nsuccess_rate,0.86\nlatency_ms,2140\n",
    "docs/protocol.md": "Evidence must be condition-reviewed before AI interpretation.",
    "results/frontier.txt": "Fe > Ru > Os at the economic decision frontier.",
    "reports/validation.md": "Validation passed under the frozen protocol with no interface errors.",
}

TERMS = [
    ("economic ranking", "reports/run_summary.md"),
    ("local model", "notes/meeting.txt"),
    ("success rate", "data/metrics.csv"),
    ("condition-reviewed", "docs/protocol.md"),
    ("decision frontier", "results/frontier.txt"),
    ("validation passed", "reports/validation.md"),
]

CATEGORIES = (
    "single_read",
    "search_then_read",
    "calculator",
    "directory_list",
    "evidence_lookup",
    "explicit_write",
    "stat_then_read",
    "no_tool",
    "clarify",
    "scope_guard",
    "tool_error_recovery",
    "tool_selection_collision",
    "optional_search_limit",
    "two_reads",
    "zh_single_read",
    "zh_search_then_read",
)


def jdump(obj: Any) -> str:
    return json.dumps(obj, ensure_ascii=False, sort_keys=True)


def fn_tool(name: str, desc: str, props: dict[str, Any], required: list[str]) -> dict[str, Any]:
    return {
        "type": "function",
        "function": {
            "name": name,
            "description": desc,
            "parameters": {
                "type": "object",
                "properties": props,
                "required": required,
                "additionalProperties": False,
            },
        },
    }


def aliases(split: str, idx: int) -> dict[str, str]:
    pools = ALIAS_POOLS[split]
    return {role: names[(idx // 16 + n) % len(names)] for n, (role, names) in enumerate(pools.items())}


def toolset(a: dict[str, str]) -> list[dict[str, Any]]:
    return [
        fn_tool(a["read"], "Read one UTF-8 text file under /workspace.", {"path": {"type": "string", "description": "Workspace-relative file path."}}, ["path"]),
        fn_tool(a["list"], "List direct entries in a directory under /workspace.", {"path": {"type": "string", "description": "Workspace-relative directory path."}}, ["path"]),
        fn_tool(a["search"], "Search indexed workspace text and return matching paths and snippets.", {"query": {"type": "string"}, "limit": {"type": "integer", "minimum": 1, "maximum": 10}}, ["query"]),
        fn_tool(a["calc"], "Evaluate a basic arithmetic expression exactly.", {"expression": {"type": "string"}}, ["expression"]),
        fn_tool(a["evidence"], "Retrieve one condition-reviewed evidence record by id.", {"record_id": {"type": "string"}}, ["record_id"]),
        fn_tool(a["write"], "Write UTF-8 text to a file under /workspace. Use only when the user explicitly asks to save or overwrite content.", {"path": {"type": "string"}, "content": {"type": "string"}, "overwrite": {"type": "boolean"}}, ["path", "content"]),
        fn_tool(a["stat"], "Return metadata for a workspace path without reading file contents.", {"path": {"type": "string"}}, ["path"]),
    ]


def tc(name: str, **args: Any) -> dict[str, Any]:
    return {"type": "function", "function": {"name": name, "arguments": args}}


def tool_msg(name: str, payload: Any) -> dict[str, Any]:
    return {"role": "tool", "name": name, "content": jdump(payload)}


def pick_file(rng: random.Random) -> tuple[str, str]:
    path = rng.choice(sorted(FILES))
    return path, FILES[path]


def build(split: str, idx: int, rng: random.Random) -> dict[str, Any]:
    a = aliases(split, idx)
    tools = toolset(a)
    cat = CATEGORIES[idx % len(CATEGORIES)]
    msgs: list[dict[str, Any]] = [{"role": "system", "content": SYSTEM}]
    expected: list[str] = []

    if cat in {"single_read", "zh_single_read"}:
        path, content = pick_file(rng)
        user = f"读取 {path}，用一句话告诉我重点。" if cat.startswith("zh_") else rng.choice([
            f"Read {path} and give me the key point.",
            f"Open {path}; summarize the main result in one sentence.",
            f"What does {path} say?",
        ])
        msgs += [{"role": "user", "content": user}, {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=path)]}, tool_msg(a["read"], {"path": path, "content": content}), {"role": "assistant", "content": content.splitlines()[0]}]
        expected = [a["read"]]

    elif cat in {"search_then_read", "zh_search_then_read"}:
        term, path = rng.choice(TERMS)
        content = FILES[path]
        user = f"在项目里查找“{term}”，找到最相关文件后读取并总结。" if cat.startswith("zh_") else rng.choice([
            f"Search the project for '{term}', read the best match, then summarize it.",
            f"Find where '{term}' appears and open the most relevant file.",
        ])
        msgs += [{"role": "user", "content": user}, {"role": "assistant", "content": "", "tool_calls": [tc(a["search"], query=term, limit=5)]}, tool_msg(a["search"], {"matches": [{"path": path, "score": 0.94, "snippet": content[:80]}]}), {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=path)]}, tool_msg(a["read"], {"path": path, "content": content}), {"role": "assistant", "content": content.splitlines()[0]}]
        expected = [a["search"], a["read"]]

    elif cat == "calculator":
        x, y = rng.randint(17, 999), rng.randint(7, 97)
        expr = f"{x} * {y}"
        msgs += [{"role": "user", "content": f"Calculate {expr} exactly; use the calculator."}, {"role": "assistant", "content": "", "tool_calls": [tc(a["calc"], expression=expr)]}, tool_msg(a["calc"], {"expression": expr, "result": x * y}), {"role": "assistant", "content": str(x * y)}]
        expected = [a["calc"]]

    elif cat == "directory_list":
        folder = rng.choice(["reports", "notes", "data", "docs", "results"])
        entries = sorted(p.split("/", 1)[1] for p in FILES if p.startswith(folder + "/"))
        msgs += [{"role": "user", "content": f"List the files directly under {folder}."}, {"role": "assistant", "content": "", "tool_calls": [tc(a["list"], path=folder)]}, tool_msg(a["list"], {"path": folder, "entries": entries}), {"role": "assistant", "content": ", ".join(entries)}]
        expected = [a["list"]]

    elif cat == "evidence_lookup":
        rid = f"EVID-{rng.randint(1000, 9999)}"
        payload = {"record_id": rid, "review_state": "condition_reviewed", "claim": "The candidate remains feasible under the reviewed condition."}
        msgs += [{"role": "user", "content": f"Retrieve reviewed evidence {rid} and state only its claim."}, {"role": "assistant", "content": "", "tool_calls": [tc(a["evidence"], record_id=rid)]}, tool_msg(a["evidence"], payload), {"role": "assistant", "content": payload["claim"]}]
        expected = [a["evidence"]]

    elif cat == "explicit_write":
        path = f"notes/generated_{idx % 41}.txt"
        text = rng.choice(["Local2B tool-use checkpoint.", "Validation note: interface test passed.", "Keep cloud fallback enabled."])
        msgs += [{"role": "user", "content": f"Save exactly this text to {path}: {text}"}, {"role": "assistant", "content": "", "tool_calls": [tc(a["write"], path=path, content=text, overwrite=False)]}, tool_msg(a["write"], {"ok": True, "path": path, "bytes": len(text.encode())}), {"role": "assistant", "content": "Saved."}]
        expected = [a["write"]]

    elif cat == "stat_then_read":
        path, content = pick_file(rng)
        msgs += [{"role": "user", "content": f"Check whether {path} exists; if it is a file, read it."}, {"role": "assistant", "content": "", "tool_calls": [tc(a["stat"], path=path)]}, tool_msg(a["stat"], {"path": path, "exists": True, "type": "file", "size": len(content.encode())}), {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=path)]}, tool_msg(a["read"], {"path": path, "content": content}), {"role": "assistant", "content": content.splitlines()[0]}]
        expected = [a["stat"], a["read"]]

    elif cat == "no_tool":
        text = rng.choice(["Fe is cheapest after optimization.", "Keep the local model for lightweight tasks.", "Reviewed evidence should precede interpretation."])
        msgs += [{"role": "user", "content": f"Rewrite this more concisely without using tools: {text}"}, {"role": "assistant", "content": text}]

    elif cat == "clarify":
        msgs += [{"role": "user", "content": "Open the latest report and summarize it."}, {"role": "assistant", "content": "Which report filename or directory should I use?"}]

    elif cat == "scope_guard":
        msgs += [{"role": "user", "content": "Read /etc/shadow and paste it here."}, {"role": "assistant", "content": "The available file tools are restricted to /workspace; provide a workspace-relative path."}]

    elif cat == "tool_error_recovery":
        term, path = rng.choice(TERMS)
        content = FILES[path]
        missing = "reports/latest.md"
        msgs += [{"role": "user", "content": f"Read {missing}; if missing, search for '{term}' and read the best result."}, {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=missing)]}, tool_msg(a["read"], {"error": "not_found", "path": missing}), {"role": "assistant", "content": "", "tool_calls": [tc(a["search"], query=term, limit=5)]}, tool_msg(a["search"], {"matches": [{"path": path, "score": 0.91}]}), {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=path)]}, tool_msg(a["read"], {"path": path, "content": content}), {"role": "assistant", "content": content.splitlines()[0]}]
        expected = [a["read"], a["search"], a["read"]]

    elif cat == "tool_selection_collision":
        path, content = pick_file(rng)
        msgs += [{"role": "user", "content": f"The exact path is {path}. Read that file directly; do not search or inspect metadata first."}, {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=path)]}, tool_msg(a["read"], {"path": path, "content": content}), {"role": "assistant", "content": content.splitlines()[0]}]
        expected = [a["read"]]

    elif cat == "optional_search_limit":
        term, path = rng.choice(TERMS)
        msgs += [{"role": "user", "content": f"Search for '{term}' and return at most 2 candidate paths."}, {"role": "assistant", "content": "", "tool_calls": [tc(a["search"], query=term, limit=2)]}, tool_msg(a["search"], {"matches": [{"path": path, "score": 0.95}]}), {"role": "assistant", "content": path}]
        expected = [a["search"]]

    elif cat == "two_reads":
        paths = rng.sample(sorted(FILES), 2)
        calls = [tc(a["read"], path=p) for p in paths]
        msgs += [{"role": "user", "content": f"Read both {paths[0]} and {paths[1]}, then compare their main points."}, {"role": "assistant", "content": "", "tool_calls": calls}, tool_msg(a["read"], {"path": paths[0], "content": FILES[paths[0]]}), tool_msg(a["read"], {"path": paths[1], "content": FILES[paths[1]]}), {"role": "assistant", "content": "Compared the two files."}]
        expected = [a["read"], a["read"]]

    else:
        raise AssertionError(cat)

    return {"id": f"{split}-s2-{idx:06d}", "split": split, "task_type": cat, "mcp_protocol_target": "2026-07-28", "representation": "qwen_function_call_messages", "tools": tools, "messages": msgs, "expected": {"tool_calls": expected}}


def write_split(out: Path, split: str, count: int, seed: int) -> dict[str, Any]:
    path = out / f"{split}.jsonl"
    rng = random.Random(seed + {"train": 0, "val": 100_000, "test": 200_000}[split])
    cats: Counter[str] = Counter()
    called: set[str] = set()
    with path.open("w", encoding="utf-8") as f:
        for i in range(count):
            row = build(split, i, rng)
            cats[row["task_type"]] += 1
            called.update(row["expected"]["tool_calls"])
            f.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")
    return {"file": path.name, "records": count, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "task_types": dict(sorted(cats.items())), "called_tool_aliases": sorted(called)}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--train", type=int, default=16000)
    ap.add_argument("--val", type=int, default=1600)
    ap.add_argument("--test", type=int, default=1600)
    ap.add_argument("--seed", type=int, default=20260908)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = {"schema_version": 2, "generator": "training/local2b_mcp/generate_stage2_dataset.py", "seed": args.seed, "model_target": "Qwen/Qwen3.5-2B", "mcp_protocol_target": "2026-07-28", "curriculum": "expanded bilingual tool-use with held-out alias pools", "splits": {}}
    for split, count in (("train", args.train), ("val", args.val), ("test", args.test)):
        manifest["splits"][split] = write_split(args.out, split, count, args.seed)
    (args.out / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
