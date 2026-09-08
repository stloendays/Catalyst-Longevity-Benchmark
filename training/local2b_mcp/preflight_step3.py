#!/usr/bin/env python3
"""CPU-safe Step-3 preflight: frozen hashes + Qwen3.5 tool chat-template validation."""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path

from transformers import AutoProcessor

FROZEN_HASHES = {
    "train.jsonl": "d2f3ec25be2599642f6355dcec997b55039137e60d9f0fb618ce18b0a7a42303",
    "val.jsonl": "89e4aca2f72fc4efd6bab0a88299a68a32cd637edc0561d7d2c95b2b27463990",
    "test.jsonl": "460b5308f6791eb53692fc0736a53e6f6b553a925207caa34517b456e28d78f2",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path: Path):
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            if line.strip():
                yield json.loads(line)


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--data-dir", type=Path, required=True)
    p.add_argument("--model", default="Qwen/Qwen3.5-2B")
    p.add_argument("--samples-per-type", type=int, default=1)
    args = p.parse_args()

    for filename, expected in FROZEN_HASHES.items():
        actual = digest(args.data_dir / filename)
        assert actual == expected, (filename, actual, expected)

    processor = AutoProcessor.from_pretrained(args.model, trust_remote_code=False)
    seen: Counter[str] = Counter()
    token_lengths: list[int] = []
    assistant_mask_supported = False
    examples = 0

    for row in rows(args.data_dir / "train.jsonl"):
        task = row["task_type"]
        if seen[task] >= args.samples_per_type:
            continue
        seen[task] += 1
        examples += 1
        rendered = processor.apply_chat_template(
            row["messages"], tools=row["tools"], tokenize=False, add_generation_prompt=False
        )
        assert isinstance(rendered, str) and len(rendered) > 20
        assert "jsonrpc" not in rendered.lower()
        enc = processor.apply_chat_template(
            row["messages"], tools=row["tools"], tokenize=True, add_generation_prompt=False, return_dict=True
        )
        ids = enc["input_ids"]
        token_lengths.append(len(ids[0]) if hasattr(ids[0], "__len__") else len(ids))
        try:
            masked = processor.apply_chat_template(
                row["messages"],
                tools=row["tools"],
                tokenize=True,
                add_generation_prompt=False,
                return_dict=True,
                return_assistant_tokens_mask=True,
            )
            mask = masked.get("assistant_masks")
            if mask is None:
                mask = masked.get("assistant_mask")
            if mask is not None:
                assistant_mask_supported = True
        except (TypeError, ValueError):
            pass

    expected_types = 16
    assert len(seen) == expected_types, sorted(seen)
    if not assistant_mask_supported:
        raise RuntimeError(
            "Qwen3.5 chat template did not expose an assistant token mask; assistant_only_loss would be unsafe"
        )

    print("STEP3_PREFLIGHT=PASS")
    print(f"task_types={len(seen)} examples={examples}")
    print(f"assistant_mask_supported={assistant_mask_supported}")
    print(f"sample_token_length_min={min(token_lengths)} max={max(token_lengths)}")


if __name__ == "__main__":
    main()
