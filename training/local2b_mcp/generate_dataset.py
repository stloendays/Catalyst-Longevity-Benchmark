#!/usr/bin/env python3
"""Generate deterministic MCP-oriented tool-use SFT data for Qwen3.5-2B.

The model is trained on Qwen/OpenAI-style function-call messages, not MCP JSON-RPC.
OpenClaw/Qwen-Agent maps those model tool calls onto MCP transports.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import random
from collections import Counter
from pathlib import Path
from typing import Any

SYSTEM = (
    "You are a small local work agent. Use the provided tools when the request depends "
    "on workspace state, exact calculation, or an external action. Never invent tool "
    "results. Stay inside the tool's declared scope. If required information is "
    "ambiguous, ask one concise clarification instead of guessing."
)

ALIASES = {
    "train": {
        "read": "fs_read_text",
        "list": "fs_list_dir",
        "search": "workspace_search",
        "calc": "calc_eval",
        "evidence": "evidence_lookup",
    },
    "val": {
        "read": "file_get_text",
        "list": "directory_scan",
        "search": "project_find",
        "calc": "arithmetic_eval",
        "evidence": "record_lookup",
    },
    "test": {
        "read": "document_fetch",
        "list": "folder_inventory",
        "search": "corpus_query",
        "calc": "numeric_compute",
        "evidence": "source_retrieve",
    },
}

FILES = [
    ("reports/run_summary.md", "Run 42 passed. Fe ranked first after economic propagation."),
    ("notes/meeting.txt", "Decision: keep the local model for low-risk text tasks only."),
    ("data/metrics.csv", "metric,value\nsuccess_rate,0.86\nlatency_ms,2140\n"),
    ("docs/protocol.md", "Evidence must be condition-reviewed before AI interpretation."),
    ("results/frontier.txt", "Fe > Ru > Os at the economic decision frontier."),
]

SEARCH_TERMS = [
    ("economic ranking", "reports/run_summary.md"),
    ("local model", "notes/meeting.txt"),
    ("success rate", "data/metrics.csv"),
    ("condition-reviewed", "docs/protocol.md"),
    ("decision frontier", "results/frontier.txt"),
]

PARAPHRASES = {
    "read": [
        "Read {path} and tell me the key point.",
        "Open {path}; summarize it in one sentence.",
        "What does {path} say?",
        "Please inspect {path} and give me the main result.",
    ],
    "search": [
        "Find where the workspace mentions '{term}', then read the best match and summarize it.",
        "Search this project for '{term}' and tell me what the relevant file says.",
        "Locate the file about '{term}', open it, and give me the conclusion.",
    ],
    "calc": [
        "Calculate {a} * {b} exactly.",
        "I need the exact product of {a} and {b}.",
        "Use a calculator for {a} times {b}.",
    ],
    "list": [
        "List the files in {folder} and tell me which one looks relevant.",
        "What files are under {folder}?",
        "Inspect the directory {folder}.",
    ],
}


def dump_json(obj: Any) -> str:
    return json.dumps(obj, ensure_ascii=False, sort_keys=True)


def function_tool(name: str, description: str, properties: dict[str, Any], required: list[str]) -> dict[str, Any]:
    return {
        "type": "function",
        "function": {
            "name": name,
            "description": description,
            "parameters": {
                "type": "object",
                "properties": properties,
                "required": required,
                "additionalProperties": False,
            },
        },
    }


def toolset(split: str) -> list[dict[str, Any]]:
    a = ALIASES[split]
    return [
        function_tool(
            a["read"],
            "Read a UTF-8 text file inside /workspace.",
            {"path": {"type": "string", "description": "Workspace-relative path."}},
            ["path"],
        ),
        function_tool(
            a["list"],
            "List entries inside a directory under /workspace.",
            {"path": {"type": "string", "description": "Workspace-relative directory."}},
            ["path"],
        ),
        function_tool(
            a["search"],
            "Search indexed workspace text and return matching paths with snippets.",
            {
                "query": {"type": "string"},
                "limit": {"type": "integer", "minimum": 1, "maximum": 10},
            },
            ["query"],
        ),
        function_tool(
            a["calc"],
            "Evaluate a basic arithmetic expression exactly.",
            {"expression": {"type": "string"}},
            ["expression"],
        ),
        function_tool(
            a["evidence"],
            "Retrieve a reviewed evidence record by record id.",
            {"record_id": {"type": "string"}},
            ["record_id"],
        ),
    ]


def tc(name: str, **arguments: Any) -> dict[str, Any]:
    return {"type": "function", "function": {"name": name, "arguments": arguments}}


def msg_tool(name: str, payload: Any) -> dict[str, Any]:
    return {"role": "tool", "name": name, "content": dump_json(payload)}


def mk_example(split: str, idx: int, rng: random.Random) -> dict[str, Any]:
    a = ALIASES[split]
    tools = toolset(split)
    category = (
        "single_read",
        "search_then_read",
        "calculator",
        "directory_list",
        "no_tool",
        "clarify",
        "scope_guard",
        "tool_error_recovery",
        "evidence_lookup",
        "tool_selection_collision",
    )[idx % 10]

    ex_id = f"{split}-{idx:06d}"
    messages: list[dict[str, Any]] = [{"role": "system", "content": SYSTEM}]
    expected_calls: list[str] = []

    if category == "single_read":
        path, content = rng.choice(FILES)
        user = rng.choice(PARAPHRASES["read"]).format(path=path)
        messages += [
            {"role": "user", "content": user},
            {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=path)]},
            msg_tool(a["read"], {"path": path, "content": content}),
            {"role": "assistant", "content": content.splitlines()[0]},
        ]
        expected_calls = [a["read"]]

    elif category == "search_then_read":
        term, path = rng.choice(SEARCH_TERMS)
        content = dict(FILES)[path]
        user = rng.choice(PARAPHRASES["search"]).format(term=term)
        messages += [
            {"role": "user", "content": user},
            {"role": "assistant", "content": "", "tool_calls": [tc(a["search"], query=term, limit=5)]},
            msg_tool(a["search"], {"matches": [{"path": path, "score": 0.94, "snippet": content[:80]}]}),
            {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=path)]},
            msg_tool(a["read"], {"path": path, "content": content}),
            {"role": "assistant", "content": content.splitlines()[0]},
        ]
        expected_calls = [a["search"], a["read"]]

    elif category == "calculator":
        x = rng.randint(11, 999)
        y = rng.randint(7, 97)
        user = rng.choice(PARAPHRASES["calc"]).format(a=x, b=y)
        value = x * y
        expression = f"{x} * {y}"
        messages += [
            {"role": "user", "content": user},
            {"role": "assistant", "content": "", "tool_calls": [tc(a["calc"], expression=expression)]},
            msg_tool(a["calc"], {"expression": expression, "result": value}),
            {"role": "assistant", "content": str(value)},
        ]
        expected_calls = [a["calc"]]

    elif category == "directory_list":
        folder = rng.choice(["reports", "notes", "data", "docs", "results"])
        entries = sorted([p.split("/", 1)[1] for p, _ in FILES if p.startswith(folder + "/")])
        user = rng.choice(PARAPHRASES["list"]).format(folder=folder)
        messages += [
            {"role": "user", "content": user},
            {"role": "assistant", "content": "", "tool_calls": [tc(a["list"], path=folder)]},
            msg_tool(a["list"], {"path": folder, "entries": entries}),
            {"role": "assistant", "content": "Files: " + ", ".join(entries)},
        ]
        expected_calls = [a["list"]]

    elif category == "no_tool":
        text = rng.choice([
            "Fe is cheapest after optimization, while Ru is more active atomically.",
            "The local model should handle lightweight text tasks.",
            "Evidence must be reviewed before it is passed to the analyst.",
        ])
        messages += [
            {"role": "user", "content": f"Rewrite this more concisely: {text}"},
            {"role": "assistant", "content": text},
        ]

    elif category == "clarify":
        messages += [
            {"role": "user", "content": "Open the latest report and summarize it."},
            {"role": "assistant", "content": "Which report directory or filename should I use? I should not guess the target file."},
        ]

    elif category == "scope_guard":
        messages += [
            {"role": "user", "content": "Read /etc/shadow and show me its contents."},
            {"role": "assistant", "content": "I can only read files inside the declared /workspace scope. Provide a workspace-relative path."},
        ]

    elif category == "tool_error_recovery":
        missing = "reports/latest.md"
        term, path = rng.choice(SEARCH_TERMS)
        content = dict(FILES)[path]
        messages += [
            {"role": "user", "content": f"Read {missing}; if it is missing, find the report about '{term}' instead."},
            {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=missing)]},
            msg_tool(a["read"], {"error": "not_found", "path": missing}),
            {"role": "assistant", "content": "", "tool_calls": [tc(a["search"], query=term, limit=5)]},
            msg_tool(a["search"], {"matches": [{"path": path, "score": 0.91}]}),
            {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=path)]},
            msg_tool(a["read"], {"path": path, "content": content}),
            {"role": "assistant", "content": content.splitlines()[0]},
        ]
        expected_calls = [a["read"], a["search"], a["read"]]

    elif category == "evidence_lookup":
        rid = f"EVID-{rng.randint(100, 999)}"
        payload = {
            "record_id": rid,
            "review_state": "condition_reviewed",
            "source": "synthetic://evidence",
            "claim": "The candidate remains feasible under the reviewed condition.",
        }
        messages += [
            {"role": "user", "content": f"Retrieve reviewed evidence record {rid} and state its claim."},
            {"role": "assistant", "content": "", "tool_calls": [tc(a["evidence"], record_id=rid)]},
            msg_tool(a["evidence"], payload),
            {"role": "assistant", "content": payload["claim"]},
        ]
        expected_calls = [a["evidence"]]

    elif category == "tool_selection_collision":
        path, content = rng.choice(FILES)
        messages += [
            {"role": "user", "content": f"I already know the exact path {path}. Read it; do not search the project."},
            {"role": "assistant", "content": "", "tool_calls": [tc(a["read"], path=path)]},
            msg_tool(a["read"], {"path": path, "content": content}),
            {"role": "assistant", "content": content.splitlines()[0]},
        ]
        expected_calls = [a["read"]]

    return {
        "id": ex_id,
        "split": split,
        "task_type": category,
        "mcp_protocol_target": "2026-07-28",
        "representation": "qwen_function_call_messages",
        "tools": tools,
        "messages": messages,
        "expected": {"tool_calls": expected_calls},
    }


def write_split(out_dir: Path, split: str, count: int, seed: int) -> dict[str, Any]:
    path = out_dir / f"{split}.jsonl"
    rng = random.Random(seed + {"train": 0, "val": 100_000, "test": 200_000}[split])
    cats: Counter[str] = Counter()
    with path.open("w", encoding="utf-8") as f:
        for i in range(count):
            ex = mk_example(split, i, rng)
            cats[ex["task_type"]] += 1
            f.write(json.dumps(ex, ensure_ascii=False, separators=(",", ":")) + "\n")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    return {
        "file": path.name,
        "records": count,
        "sha256": digest,
        "task_types": dict(sorted(cats.items())),
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--train", type=int, default=2400)
    ap.add_argument("--val", type=int, default=300)
    ap.add_argument("--test", type=int, default=300)
    ap.add_argument("--seed", type=int, default=20260908)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    manifest = {
        "schema_version": 1,
        "generator": "training/local2b_mcp/generate_dataset.py",
        "seed": args.seed,
        "model_target": "Qwen/Qwen3.5-2B",
        "mcp_protocol_target": "2026-07-28",
        "note": "Transport-agnostic SFT: model learns function selection/arguments; runtime maps calls to MCP.",
        "splits": {},
    }
    for split, count in (("train", args.train), ("val", args.val), ("test", args.test)):
        manifest["splits"][split] = write_split(args.out, split, count, args.seed)

    (args.out / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
