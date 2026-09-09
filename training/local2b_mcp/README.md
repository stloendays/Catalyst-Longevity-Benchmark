# Local2B MCP LoRA — Data, Training, and Benchmark Contract

Status: **Step 1 and Step 2 frozen; Step 3A verified complete; Step 3B server continuation in progress**

## Goal

Specialize the deployed `Qwen/Qwen3.5-2B` model for reliable small-agent tool use while preserving conversational behavior. The target runtime is OpenClaw with MCP-backed tools.

The model is **not** trained to emit MCP JSON-RPC packets. It is trained to produce structured function calls from tool schemas. OpenClaw or Qwen-Agent owns MCP discovery, transport, authorization, `tools/call`, and tool-result delivery. The dataset protocol reference is MCP `2026-07-28`.

## Training representation

The source datasets use semantic Qwen/OpenAI-style messages. For the server-native QVAC path, `prepare_qvac_sft.py` converts records to Qwen3.5-compatible messages-only JSONL, injects the compact tool contract into the system turn, serializes assistant calls with native Qwen3.5 XML tool-call syntax, and serializes tool results as `<tool_response>` user turns. Loss is assistant-only. No chain-of-thought labels and no raw MCP transport packets are trained.

## Step 1 — seed curriculum

The deterministic Step-1 generator produces 3,000 high-precision examples:

- train: 2,400
- validation: 300
- test: 300
- seed: `20260908`

It covers direct reads, search→read, exact arithmetic, directory listing, no-tool answers, clarification, scope guarding, tool-error recovery, evidence retrieval, and collision between similar tools.

## Step 2 — expanded curriculum

The frozen Step-2 generator produces **19,200 records**:

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

Train, validation, and test use disjoint tool alias pools.

Frozen dataset hashes:

- train: `d2f3ec25be2599642f6355dcec997b55039137e60d9f0fb618ce18b0a7a42303`
- validation: `89e4aca2f72fc4efd6bab0a88299a68a32cd637edc0561d7d2c95b2b27463990`
- test: `460b5308f6791eb53692fc0736a53e6f6b553a925207caa34517b456e28d78f2`

## Frozen pre-LoRA baseline

The unmodified deployed `Qwen3.5-2B-Q4_K_M` was evaluated directly through llama.cpp `/v1/chat/completions` with `tools` and `tool_choice=auto`.

| Metric | Base Q4 |
|---|---:|
| Tool gate accuracy | 90.48% |
| Tool sequence exact accuracy | 96.67% |
| Exact tool call + arguments | 73.33% |
| Valid argument shape | 97.78% |
| No-tool accuracy | 40.00% |
| False tool-call rate on no-tool cases | 60.00% |
| Mean decision latency | 5.47 s |

The main Step-3 targets are clarification/scope gating, exact write arguments, exact later-step arguments, and English/Chinese search→read trajectories. The frozen machine-readable baseline is `baselines/base_q4_step2.json`.

## Step 3 — selected training route

The repository retains the standard Hugging Face PEFT/QLoRA implementation for GPU environments. The deployed Tencent host has no NVIDIA GPU and only about 3.6 GiB RAM, so the selected production route is server-native CPU LoRA against the existing Q4 GGUF with the QVAC llama.cpp fork.

Pinned trainer source:

- repository: `tetherto/qvac-fabric-llm.cpp`
- commit: `657fb2ec6a841f259b4d3d20861864323d5f2737`
- portable trainer on server: `/home/ubuntu/qvac-trainer-step3/llama-finetune-lora`
- base GGUF SHA256: `d6bd9f4175302658c1d704e44858842606652bbe53c56cd542e31ff10d0cfd58`
- Qwen3.5 requirement: `-fa off`
- loss: assistant-only

This path intentionally leaves the deployed base Q4 GGUF untouched. Training writes separate GGUF LoRA adapters.

### Step 3A — verified complete

Stage 3A used rank 4 / alpha 8 LoRA on `attn_q,attn_v`, seed `20260908`, learning rate `1e-5`, one epoch, context 512, and CPU-only execution.

Verified results:

- train progress: 448 / 448
- train loss: `0.03974`
- train token accuracy: `95.34%`
- validation loss: `0.05669`
- validation token accuracy: `95.83%`
- adapter bytes: `837408`
- adapter SHA256: `db8b9d9509c752941c47261017e52dab0a9590bc1d4f94dc09fc2e061fa0eb27`
- installed path: `/opt/rhocodec-llm/adapters/local2b-mcp-step3a-r4-qv.gguf`
- strict inference smoke: final assistant content exactly `STAGE3A_SMOKE_OK`
- base inference service restored healthy after smoke

A separate 4-record continuation smoke then verified that QVAC can train an existing LoRA with `--lora`. It logged `Finetuning existing LoRA adapters` / `Found 1 existing LoRA adapters to train`, consumed the exact Stage-3A SHA above, produced a new 837408-byte adapter, and ended with `STAGE3B_CONTINUATION_SMOKE=PASS` while restoring the base service.

### Step 3B — formal continuation in progress

The frozen Stage-3B curriculum contains 64 targeted records and has SHA256:

`0ea19f7143ebcf2de083a4c62ea076050032c49cd6d6e3e6073bf5b0bfc3528c`

Task quotas emphasize the frozen baseline weaknesses: clarification, scope guarding, explicit writes, no-tool negatives, search→read, bilingual search→read, error recovery, stat→read, two-read, and tool-selection collision.

Formal continuation configuration:

- initialization adapter: verified Stage-3A adapter above
- context: 576
- batch / microbatch: 16 / 8
- rank / alpha: 4 / 8
- modules: `attn_q,attn_v`
- learning rate: `1e-5`
- epochs: 1
- seed: `20260908`
- checkpoint interval: 10 steps
- serialized with GitHub Actions concurrency plus server `flock`
- intended output: `/home/ubuntu/local2b-training/step3b/local2b-mcp-step3b-r4-qv.gguf`
- intended installed adapter: `/opt/rhocodec-llm/adapters/local2b-mcp-step3b-r4-qv.gguf`

The final Stage-3B adapter SHA and losses are intentionally not recorded until the formal run completes and is verified.

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

### Step 3 — LoRA training **IN PROGRESS**
Stage 3A is complete and verified. Stage 3B continues from the Stage-3A adapter on the server-native QVAC route.

### Step 4 — adapter evaluation + deployment export
Evaluate the final adapter on the same frozen benchmark, verify load/merge behavior, and prepare the llama.cpp/OpenClaw deployment while preserving the base model for rollback.

### Step 5 — OpenClaw MCP agent integration
Attach a narrow MCP tool inventory to a dedicated OpenClaw agent and run live end-to-end MCP tasks.

### Step 6 — A/B deployment
Compare base vs LoRA on the frozen test set and real project tasks, then promote only if reliability improves without unacceptable language regression.

## Key files

- `generate_dataset.py` — deterministic Step-1 generator
- `generate_stage2_dataset.py` — expanded Step-2 curriculum
- `validate_dataset.py` — schema/trajectory/split validator
- `benchmark_tool_use.py` — deterministic direct llama.cpp decision benchmark
- `prepare_qvac_sft.py` — QVAC/Qwen3.5 SFT converter
- `prepare_qvac_sft_stage3b.py` — frozen Stage-3B targeted curriculum selector
- `run_server_qvac_lora.sh` — reusable checkpointed server-native LoRA runner
- `baselines/base_q4_step2.json` — frozen pre-LoRA measurement
- `.github/workflows/local2b-mcp-step3a-finalize.yml` — strict Stage-3A adapter installation/inference verification
- `.github/workflows/local2b-mcp-step3b-continuation-smoke.yml` — existing-LoRA continuation verification
- `.github/workflows/local2b-mcp-step3b-server.yml` — formal serialized Stage-3B continuation
