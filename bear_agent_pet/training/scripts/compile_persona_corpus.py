#!/usr/bin/env python3
"""Compile Tony's editable persona source corpus into chat-SFT JSONL splits."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def stable_bucket(text: str, buckets: int = 10) -> int:
    digest = hashlib.sha256(text.encode("utf-8")).digest()
    return int.from_bytes(digest[:4], "big") % buckets


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        default="bear_agent_pet/training/source/tony_persona_en_v1.json",
    )
    parser.add_argument(
        "--output-dir",
        default="bear_agent_pet/training/processed/tony_persona_en_v1",
    )
    args = parser.parse_args()

    source_path = Path(args.source)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    data = json.loads(source_path.read_text(encoding="utf-8"))
    system = data["system"].strip()
    examples = data["examples"]

    seen = set()
    rows = []
    for index, example in enumerate(examples):
        user = example["user"].strip()
        assistant = example["assistant"].strip()
        key = user.casefold()
        if not user or not assistant:
            raise ValueError(f"Empty user/assistant field at example {index}")
        if key in seen:
            raise ValueError(f"Duplicate user prompt: {user}")
        seen.add(key)

        row = {
            "messages": [
                {"role": "system", "content": system},
                {"role": "user", "content": user},
                {"role": "assistant", "content": assistant},
            ],
            "tags": example.get("tags", []),
            "action": example.get("action", "idle"),
            "emotion": example.get("emotion", "neutral"),
            "source": "tony_persona_en_v1",
            "weight": 1.0,
        }
        rows.append(row)

    train_rows = []
    eval_rows = []
    for row in rows:
        prompt = row["messages"][1]["content"]
        # Deterministic ~12.5% holdout. The split stays stable as files move.
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

    manifest = {
        "name": data.get("name"),
        "version": data.get("version"),
        "language": data.get("language"),
        "total": len(rows),
        "train": len(train_rows),
        "eval": len(eval_rows),
        "split": "deterministic_sha256_bucket_1_of_8_eval",
        "source_file": source_path.as_posix(),
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
