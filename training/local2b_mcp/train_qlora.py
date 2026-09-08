#!/usr/bin/env python3
"""Step-3 QLoRA trainer for Qwen3.5-2B tool-use specialization.

This trainer consumes the frozen Step-2 JSONL curriculum and applies LoRA only
to 4-bit linear layers inside the Qwen3.5 language tower. The visual tower is
left untouched. It is intended for a single CUDA GPU (T4/L4/A10G-class or
better) and TRL's native tool-calling SFT path.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import random
import sys
from collections import Counter
from pathlib import Path
from typing import Any

import torch
from datasets import Dataset
from peft import LoraConfig, prepare_model_for_kbit_training
from transformers import AutoProcessor, BitsAndBytesConfig, Qwen3_5ForConditionalGeneration
from trl import SFTConfig, SFTTrainer

DEFAULT_WEIGHTS = {
    "clarify": 3,
    "scope_guard": 3,
    "no_tool": 2,
    "explicit_write": 2,
    "search_then_read": 2,
    "zh_search_then_read": 2,
    "tool_error_recovery": 2,
    "stat_then_read": 2,
    "two_reads": 2,
}

FROZEN_HASHES = {
    "train": "d2f3ec25be2599642f6355dcec997b55039137e60d9f0fb618ce18b0a7a42303",
    "val": "89e4aca2f72fc4efd6bab0a88299a68a32cd637edc0561d7d2c95b2b27463990",
    "test": "460b5308f6791eb53692fc0736a53e6f6b553a925207caa34517b456e28d78f2",
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
                rows.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_no}: invalid JSON") from exc
    return rows


def verify_frozen_dataset(data_dir: Path) -> None:
    for split, expected in FROZEN_HASHES.items():
        path = data_dir / ("val.jsonl" if split == "val" else f"{split}.jsonl")
        actual = sha256(path)
        if actual != expected:
            raise RuntimeError(f"Frozen dataset hash mismatch for {split}: {actual} != {expected}")


def parse_weights(value: str | None) -> dict[str, int]:
    weights = dict(DEFAULT_WEIGHTS)
    if not value:
        return weights
    custom = json.loads(value)
    if not isinstance(custom, dict):
        raise ValueError("--task-weights-json must decode to an object")
    for key, raw in custom.items():
        repeat = int(raw)
        if repeat < 1 or repeat > 8:
            raise ValueError(f"Invalid repeat weight for {key}: {repeat}")
        weights[str(key)] = repeat
    return weights


def expand_training_rows(
    rows: list[dict[str, Any]], weights: dict[str, int], seed: int, limit: int | None
) -> tuple[list[dict[str, Any]], Counter[str]]:
    if limit is not None:
        rows = rows[:limit]
    expanded: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()
    for row in rows:
        task_type = str(row.get("task_type", "unknown"))
        repeat = int(weights.get(task_type, 1))
        counts[task_type] += repeat
        for copy_idx in range(repeat):
            item = dict(row)
            item["_repeat_index"] = copy_idx
            expanded.append(item)
    random.Random(seed).shuffle(expanded)
    return expanded, counts


def choose_compute_dtype() -> torch.dtype:
    return torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16


def discover_language_linear_targets(model: torch.nn.Module) -> list[str]:
    linear_types: tuple[type[Any], ...] = (torch.nn.Linear,)
    try:
        import bitsandbytes as bnb

        linear_types = linear_types + (bnb.nn.Linear4bit,)
    except Exception:
        pass

    targets: list[str] = []
    prefix = "model.language_model."
    for name, module in model.named_modules():
        if name.startswith(prefix) and isinstance(module, linear_types):
            targets.append(name)
    if not targets:
        raise RuntimeError("No quantized/linear modules found under model.language_model")
    if any("visual" in name for name in targets):
        raise RuntimeError("Vision module leaked into language-only LoRA target list")
    return sorted(set(targets))


def build_dataset(rows: list[dict[str, Any]]) -> Dataset:
    clean = []
    for row in rows:
        clean.append({"messages": row["messages"], "tools": row["tools"], "task_type": row["task_type"]})
    return Dataset.from_list(clean)


def write_metadata(
    out_dir: Path,
    args: argparse.Namespace,
    dtype: torch.dtype,
    targets: list[str],
    train_rows: int,
    eval_rows: int,
    task_counts: Counter[str],
) -> None:
    meta = {
        "stage": 3,
        "status": "training_started",
        "model": args.model,
        "seed": args.seed,
        "dataset_hashes": FROZEN_HASHES,
        "train_rows_after_weighting": train_rows,
        "eval_rows": eval_rows,
        "weighted_task_counts": dict(sorted(task_counts.items())),
        "compute_dtype": str(dtype).replace("torch.", ""),
        "quantization": {
            "load_in_4bit": True,
            "bnb_4bit_quant_type": "nf4",
            "bnb_4bit_use_double_quant": True,
        },
        "lora": {
            "r": args.lora_r,
            "alpha": args.lora_alpha,
            "dropout": args.lora_dropout,
            "target_scope": "model.language_model.* linear modules only",
            "target_count": len(targets),
            "targets": targets,
        },
        "sft": {
            "assistant_only_loss": True,
            "max_length": args.max_length,
            "packing": False,
            "learning_rate": args.learning_rate,
            "batch_size": args.batch_size,
            "gradient_accumulation_steps": args.gradient_accumulation_steps,
            "num_train_epochs": args.epochs,
            "max_steps": args.max_steps,
        },
        "library_versions": {
            "torch": torch.__version__,
        },
    }
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "step3_training_metadata.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser()
    p.add_argument("--data-dir", type=Path, required=True)
    p.add_argument("--output-dir", type=Path, required=True)
    p.add_argument("--model", default="Qwen/Qwen3.5-2B")
    p.add_argument("--seed", type=int, default=20260908)
    p.add_argument("--train-limit", type=int, default=None)
    p.add_argument("--eval-limit", type=int, default=400)
    p.add_argument("--max-length", type=int, default=2048)
    p.add_argument("--batch-size", type=int, default=1)
    p.add_argument("--gradient-accumulation-steps", type=int, default=16)
    p.add_argument("--epochs", type=float, default=1.0)
    p.add_argument("--max-steps", type=int, default=-1)
    p.add_argument("--learning-rate", type=float, default=1e-4)
    p.add_argument("--warmup-ratio", type=float, default=0.05)
    p.add_argument("--lora-r", type=int, default=16)
    p.add_argument("--lora-alpha", type=int, default=32)
    p.add_argument("--lora-dropout", type=float, default=0.05)
    p.add_argument("--logging-steps", type=int, default=10)
    p.add_argument("--eval-steps", type=int, default=100)
    p.add_argument("--save-steps", type=int, default=100)
    p.add_argument("--task-weights-json", default=None)
    p.add_argument("--no-hash-check", action="store_true")
    p.add_argument("--resume-from-checkpoint", default=None)
    return p.parse_args()


def main() -> None:
    args = parse_args()
    if not torch.cuda.is_available():
        raise RuntimeError("Step-3 QLoRA requires a CUDA GPU")
    if not args.no_hash_check:
        verify_frozen_dataset(args.data_dir)

    torch.manual_seed(args.seed)
    random.seed(args.seed)

    train_rows = load_jsonl(args.data_dir / "train.jsonl")
    eval_rows = load_jsonl(args.data_dir / "val.jsonl")
    weights = parse_weights(args.task_weights_json)
    train_rows, task_counts = expand_training_rows(train_rows, weights, args.seed, args.train_limit)
    if args.eval_limit is not None:
        eval_rows = eval_rows[: args.eval_limit]

    train_ds = build_dataset(train_rows)
    eval_ds = build_dataset(eval_rows)

    compute_dtype = choose_compute_dtype()
    quant_config = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_quant_type="nf4",
        bnb_4bit_compute_dtype=compute_dtype,
        bnb_4bit_use_double_quant=True,
    )

    processor = AutoProcessor.from_pretrained(args.model, trust_remote_code=False)
    if getattr(processor, "tokenizer", None) is not None:
        tok = processor.tokenizer
        if tok.pad_token_id is None:
            tok.pad_token = tok.eos_token
        tok.padding_side = "right"

    model = Qwen3_5ForConditionalGeneration.from_pretrained(
        args.model,
        quantization_config=quant_config,
        device_map={"": 0},
        torch_dtype=compute_dtype,
        low_cpu_mem_usage=True,
    )
    model.config.use_cache = False
    model = prepare_model_for_kbit_training(model, use_gradient_checkpointing=True)
    targets = discover_language_linear_targets(model)

    lora = LoraConfig(
        r=args.lora_r,
        lora_alpha=args.lora_alpha,
        lora_dropout=args.lora_dropout,
        bias="none",
        task_type="CAUSAL_LM",
        target_modules=targets,
    )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    write_metadata(args.output_dir, args, compute_dtype, targets, len(train_ds), len(eval_ds), task_counts)

    sft_args = SFTConfig(
        output_dir=str(args.output_dir),
        seed=args.seed,
        data_seed=args.seed,
        per_device_train_batch_size=args.batch_size,
        per_device_eval_batch_size=1,
        gradient_accumulation_steps=args.gradient_accumulation_steps,
        gradient_checkpointing=True,
        gradient_checkpointing_kwargs={"use_reentrant": False},
        learning_rate=args.learning_rate,
        warmup_ratio=args.warmup_ratio,
        lr_scheduler_type="cosine",
        optim="paged_adamw_8bit",
        num_train_epochs=args.epochs,
        max_steps=args.max_steps,
        max_length=args.max_length,
        packing=False,
        assistant_only_loss=True,
        logging_steps=args.logging_steps,
        eval_strategy="steps",
        eval_steps=args.eval_steps,
        save_strategy="steps",
        save_steps=args.save_steps,
        save_total_limit=2,
        report_to="none",
        fp16=compute_dtype == torch.float16,
        bf16=compute_dtype == torch.bfloat16,
        max_grad_norm=1.0,
        weight_decay=0.0,
        remove_unused_columns=False,
        dataset_num_proc=min(4, os.cpu_count() or 1),
    )

    trainer = SFTTrainer(
        model=model,
        args=sft_args,
        train_dataset=train_ds,
        eval_dataset=eval_ds,
        processing_class=processor,
        peft_config=lora,
    )
    trainer.model.print_trainable_parameters()
    train_result = trainer.train(resume_from_checkpoint=args.resume_from_checkpoint)
    trainer.save_model(str(args.output_dir / "adapter"))
    processor.save_pretrained(str(args.output_dir / "adapter"))
    trainer.save_state()

    metrics = dict(train_result.metrics)
    metrics["train_rows_after_weighting"] = len(train_ds)
    metrics["eval_rows"] = len(eval_ds)
    metrics["language_lora_target_count"] = len(targets)
    (args.output_dir / "train_metrics.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print("STEP3_TRAINING_COMPLETE")
    print(json.dumps(metrics, indent=2))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"STEP3_TRAINING_FAILED: {type(exc).__name__}: {exc}", file=sys.stderr)
        raise
