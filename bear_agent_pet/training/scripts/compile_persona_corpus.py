#!/usr/bin/env python3
"""Compile Tony's English persona corpus into deterministic chat-SFT splits."""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path

from persona_expansion import build_examples as build_expansion_examples


def stable_bucket(text: str, buckets: int = 10) -> int:
    digest = hashlib.sha256(text.encode("utf-8")).digest()
    return int.from_bytes(digest[:4], "big") % buckets


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        default="bear_agent_pet/training/source/tony_persona_en_v1.json",
        help="High-weight hand-curated persona seed JSON.",
    )
    parser.add_argument(
        "--output-dir",
        default="bear_agent_pet/training/processed/tony_persona_en_v2",
    )
    parser.add_argument(
        "--no-expansion",
        action="store_true",
        help="Compile only the hand-curated seed examples.",
    )
    args = parser.parse_args()

    source_path = Path(args.source)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    data = json.loads(source_path.read_text(encoding="utf-8"))
    system = data["system"].strip()
    base_examples = data["examples"]
    expansion_examples = [] if args.no_expansion else build_expansion_examples()

    seen: set[str] = set()
    rows: list[dict] = []
    skipped_duplicates: list[str] = []

    def ingest(examples: list[dict], source_name: str, default_weight: float) -> None:
        for index, example in enumerate(examples):
            user = example["user"].strip()
            assistant = example["assistant"].strip()
            key = user.casefold()
            if not user or not assistant:
                raise ValueError(f"Empty user/assistant field at {source_name}:{index}")
            if key in seen:
                # The curated seed always wins over a synthetic expansion phrasing.
                skipped_duplicates.append(user)
                continue
            seen.add(key)

            rows.append({
                "messages": [
                    {"role": "system", "content": system},
                    {"role": "user", "content": user},
                    {"role": "assistant", "content": assistant},
                ],
                "tags": example.get("tags", []),
                "action": example.get("action", "idle"),
                "emotion": example.get("emotion", "neutral"),
                "source": source_name,
                "weight": float(example.get("weight", default_weight)),
            })

    ingest(base_examples, "tony_persona_en_v1_curated", 1.0)
    ingest(expansion_examples, "tony_persona_en_v2_expansion", 0.8)

    train_rows: list[dict] = []
    eval_rows: list[dict] = []
    for row in rows:
        prompt = row["messages"][1]["content"]
        # Deterministic 1/8 holdout. The split remains stable across machines/runs.
        if stable_bucket(prompt, 8) == 0:
            eval_rows.append(row)
        else:
            train_rows.append(row)

    def write_jsonl(path: Path, values: list[dict]) -> None:
        with path.open("w", encoding="utf-8", newline="\n") as handle:
            for value in values:
                handle.write(json.dumps(value, ensure_ascii=False) + "\n")

    write_jsonl(output_dir / "all.jsonl", rows)
    write_jsonl(output_dir / "train.jsonl", train_rows)
    write_jsonl(output_dir / "eval.jsonl", eval_rows)

    action_counts = Counter(row["action"] for row in rows)
    tag_counts = Counter(tag for row in rows for tag in row.get("tags", []))
    source_counts = Counter(row["source"] for row in rows)
    manifest = {
        "name": "Tony English Persona Corpus",
        "version": "2.0",
        "language": data.get("language", "en"),
        "total": len(rows),
        "train": len(train_rows),
        "eval": len(eval_rows),
        "split": "deterministic_sha256_bucket_1_of_8_eval",
        "source_file": source_path.as_posix(),
        "source_counts": dict(sorted(source_counts.items())),
        "action_counts": dict(sorted(action_counts.items())),
        "tag_counts": dict(sorted(tag_counts.items())),
        "skipped_duplicate_prompts": skipped_duplicates,
        "notes": "Curated v1 examples have weight 1.0; deterministic v2 expansion examples have weight 0.8.",
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
