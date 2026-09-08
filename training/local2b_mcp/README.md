# Local2B MCP LoRA — Step 1: Data Contract

Status: **frozen for Step 1**

## Goal

Specialize the currently deployed `Qwen/Qwen3.5-2B` checkpoint for reliable small-agent tool use while preserving its existing conversational behavior. The target runtime is OpenClaw with MCP-backed tools.

The model is **not** trained to emit MCP JSON-RPC packets. It is trained to produce Qwen/OpenAI-style structured function calls from tool schemas. OpenClaw or Qwen-Agent owns MCP discovery, transport, authorization, `tools/call`, and tool-result delivery. This keeps the LoRA compatible with both OpenClaw and other Qwen tool runtimes and avoids coupling model weights to one MCP wire version.

Protocol reference for the dataset manifest: MCP `2026-07-28`.

## Why this representation

`Qwen/Qwen3.5-2B` already ships a chat template that accepts a `tools` list and renders assistant `tool_calls` plus tool responses. Training on the same semantic message structure lets the tokenizer/chat template produce the exact model-specific control format at preprocessing time.

Canonical record shape:

```json
{
  "id": "train-000001",
  "split": "train",
  "task_type": "search_then_read",
  "mcp_protocol_target": "2026-07-28",
  "representation": "qwen_function_call_messages",
  "tools": [{"type": "function", "function": {"name": "...", "parameters": {}}}],
  "messages": [
    {"role": "system", "content": "..."},
    {"role": "user", "content": "..."},
    {"role": "assistant", "content": "", "tool_calls": [{"type": "function", "function": {"name": "...", "arguments": {}}}]},
    {"role": "tool", "name": "...", "content": "{...}"},
    {"role": "assistant", "content": "..."}
  ],
  "expected": {"tool_calls": ["..."]}
}
```

During LoRA preprocessing, `processor.apply_chat_template(messages, tools=tools, ...)` will render this into Qwen3.5's native tool-call syntax. No chain-of-thought labels are generated or trained.

## Step-1 synthetic curriculum

The deterministic generator produces 3,000 high-precision examples by default:

- train: 2,400
- validation: 300
- test: 300

Ten task families are balanced by construction:

1. `single_read` — exact target → direct file read.
2. `search_then_read` — search, inspect result, then read.
3. `calculator` — use exact arithmetic instead of guessing.
4. `directory_list` — inspect workspace state.
5. `no_tool` — answer directly when a tool is unnecessary.
6. `clarify` — ask one concise question instead of guessing an ambiguous target.
7. `scope_guard` — do not call a tool outside its declared scope.
8. `tool_error_recovery` — recover from a failed call using another tool.
9. `evidence_lookup` — retrieve a reviewed record and use only returned evidence.
10. `tool_selection_collision` — choose a direct read over search when the exact path is already known.

## Generalization guard

Train, validation, and test use **disjoint tool names** while retaining semantically equivalent JSON schemas. Example:

- train: `fs_read_text`
- validation: `file_get_text`
- test: `document_fetch`

A model that memorizes tool names will therefore fail held-out evaluation. A model that conditions on tool descriptions and schemas should transfer.

## Quality rules

Every generated record must satisfy all of the following:

- every tool call names a tool present in that record's tool inventory;
- arguments are structured objects;
- every tool result corresponds to a preceding tool call;
- no assistant message emits raw MCP `jsonrpc` / `tools/call` transport text;
- train/validation/test ids are unique;
- training tool aliases do not occur in validation/test calls;
- tool-free, clarification, and scope-guard examples contain no hidden tool call;
- generation is deterministic from a frozen seed.

## Frozen Step-1 seed

`20260908`

## Multi-step project plan

### Step 1 — data contract + deterministic seed curriculum **(current)**
Create the schema, generator, validator and CI artifact. Establish a clean held-out benchmark before training.

### Step 2 — dataset expansion + base-model benchmark
Add paraphrase diversity, distractor tools, multi-server MCP inventories, malformed/partial tool results and longer trajectories. Benchmark unmodified Qwen3.5-2B first so LoRA improvement is measurable.

### Step 3 — LoRA / QLoRA training
Fine-tune the Hugging Face checkpoint with PEFT. Freeze training config, seed, adapter rank/alpha/dropout, target modules, max sequence length, gradient accumulation and evaluation cadence. Do not train the GGUF directly.

### Step 4 — adapter evaluation + export
Measure tool-selection accuracy, exact argument accuracy, invalid-call rate, no-tool precision, recovery success and held-out tool-name transfer. Merge or load the adapter, then export a llama.cpp-compatible GGUF and quantize to the target Q4 profile.

### Step 5 — OpenClaw MCP agent integration
Attach a narrow MCP tool inventory to a dedicated OpenClaw agent, run live MCP probes and complete end-to-end tasks. Keep the original model available for rollback.

### Step 6 — A/B deployment
Compare base vs LoRA agent on the frozen test set and real project tasks, then promote only if the LoRA improves tool reliability without unacceptable language regression.

## Files

- `generate_dataset.py` — deterministic synthetic data generator.
- `validate_dataset.py` — schema/trajectory/split validator.
- `.github/workflows/local2b-mcp-data-step1.yml` — generates and validates the frozen 3k-record seed dataset as a CI artifact.
