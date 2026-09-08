# Local2B MCP LoRA — Data and Benchmark Contract

Status: **Step 1 and Step 2 frozen; Step 3 is LoRA training**

## Goal

Specialize the currently deployed `Qwen/Qwen3.5-2B` checkpoint for reliable small-agent tool use while preserving its existing conversational behavior. The target runtime is OpenClaw with MCP-backed tools.

The model is **not** trained to emit MCP JSON-RPC packets. It is trained to produce Qwen/OpenAI-style structured function calls from tool schemas. OpenClaw or Qwen-Agent owns MCP discovery, transport, authorization, `tools/call`, and tool-result delivery. This keeps the LoRA compatible with both OpenClaw and other Qwen tool runtimes and avoids coupling model weights to one MCP wire version.

Protocol reference for the dataset manifest: MCP `2026-07-28`.

## Training representation

`Qwen/Qwen3.5-2B` ships a chat template that accepts a `tools` list and renders assistant `tool_calls` plus tool responses. Training on the same semantic message structure lets the tokenizer/chat template produce the model-specific control format at preprocessing time.

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

During LoRA preprocessing, `processor.apply_chat_template(messages, tools=tools, ...)` renders this into Qwen3.5 native tool-call syntax. No chain-of-thought labels are generated or trained.

## Step 1 — seed curriculum

The deterministic Step-1 generator produces 3,000 high-precision examples:

- train: 2,400
- validation: 300
- test: 300
- seed: `20260908`

It covers direct reads, search→read, exact arithmetic, directory listing, no-tool answers, clarification, scope guarding, tool-error recovery, evidence retrieval, and collision between similar tools.

## Step 2 — expanded curriculum

The frozen Step-2 generator expands the curriculum to **19,200 records**:

- train: 16,000
- validation: 1,600
- test: 1,600
- 16 balanced task families
- bilingual English/Chinese instructions
- seven semantic tool roles: read, list, search, calculate, evidence, write, stat
- optional parameters and exact argument constraints
- direct-vs-search and stat-vs-read distractors
- multi-step recovery trajectories
- parallel/two-read calls
- explicit write operations
- no-tool, clarification, and scope-guard negatives

Train, validation, and test use disjoint tool alias pools. Tool names in the Step-2 test set are never used as called tool names in training.

Frozen dataset hashes:

- train: `d2f3ec25be2599642f6355dcec997b55039137e60d9f0fb618ce18b0a7a42303`
- validation: `89e4aca2f72fc4efd6bab0a88299a68a32cd637edc0561d7d2c95b2b27463990`
- test: `460b5308f6791eb53692fc0736a53e6f6b553a925207caa34517b456e28d78f2`

## Frozen pre-LoRA baseline

The deployed unmodified `Qwen3.5-2B-Q4_K_M` was evaluated directly through llama.cpp `/v1/chat/completions` with `tools` and `tool_choice=auto`. OpenClaw was deliberately bypassed so this measures model-level tool behavior rather than agent-prompt overhead.

Benchmark sample:

- 80 balanced held-out Step-2 test records
- 105 assistant decisions
- 90 tool-required decisions
- 15 no-tool decisions
- temperature: 0
- API/parse errors: 0

Results:

| Metric | Base Q4 |
|---|---:|
| Tool gate accuracy | 90.48% |
| Tool sequence exact accuracy | 96.67% |
| Exact tool call + arguments | 73.33% |
| Valid argument shape | 97.78% |
| No-tool accuracy | 40.00% |
| False tool-call rate on no-tool cases | 60.00% |
| Mean decision latency | 5.47 s |

The base model already transfers tool selection to unseen aliases. The main reliability deficits are now well defined:

1. `clarify`: 0/5 correctly avoided tool invocation.
2. `scope_guard`: only 1/5 correctly avoided an out-of-scope call.
3. `explicit_write`: only 1/5 achieved exact arguments.
4. Multi-step trajectories often choose the right tool but drift in exact later-step arguments.
5. Search→read exact calls were 5/10 in English and 5/10 in Chinese.

These deficits become the primary Step-3 LoRA targets. The frozen machine-readable baseline is `baselines/base_q4_step2.json`.

## Generalization guard

Train, validation, and test use disjoint tool names while retaining semantically equivalent JSON schemas. The benchmark therefore rewards reading tool descriptions and schemas rather than memorizing names.

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

## Multi-step project plan

### Step 1 — data contract + deterministic seed curriculum **DONE**
Create the schema, generator, validator and CI artifact.

### Step 2 — dataset expansion + base-model benchmark **DONE**
Expand to 19.2k bilingual/distractor/multi-step examples and freeze the pre-LoRA baseline.

### Step 3 — LoRA / QLoRA training **NEXT**
Fine-tune the Hugging Face `Qwen/Qwen3.5-2B` checkpoint with PEFT. Freeze training config, seed, adapter rank/alpha/dropout, target modules, max sequence length, gradient accumulation and evaluation cadence. Do not train the GGUF directly.

### Step 4 — adapter evaluation + export
Measure the same frozen metrics, merge or load the adapter, export a llama.cpp-compatible GGUF, and quantize to the target Q4 profile.

### Step 5 — OpenClaw MCP agent integration
Attach a narrow MCP tool inventory to a dedicated OpenClaw agent, run live MCP probes and complete end-to-end tasks. Keep the original model available for rollback.

### Step 6 — A/B deployment
Compare base vs LoRA on the frozen test set and real project tasks, then promote only if tool reliability improves without unacceptable language regression.

## Files

- `generate_dataset.py` — deterministic Step-1 seed generator.
- `generate_stage2_dataset.py` — expanded Step-2 curriculum generator.
- `validate_dataset.py` — schema/trajectory/split validator.
- `benchmark_tool_use.py` — deterministic direct llama.cpp decision benchmark.
- `baselines/base_q4_step2.json` — frozen pre-LoRA measurement.
- `.github/workflows/local2b-mcp-data-step1.yml` — Step-1 generation/validation CI.
- `.github/workflows/local2b-mcp-step2-baseline.yml` — Step-2 dataset generation and base-model benchmark CI.
