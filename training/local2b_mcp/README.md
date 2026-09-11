# Local2B MCP LoRA — Data, Training, and Benchmark Contract

Status: **Step 1–2 frozen; Stage 3B rejected by held-out evaluation; Stage 3C fresh recovery training in progress**

## Goal

Specialize the deployed `Qwen/Qwen3.5-2B` model for reliable small-agent tool use while preserving conversational behavior. The target runtime is OpenClaw with MCP-backed tools.

The model is **not** trained to emit MCP JSON-RPC packets. It is trained to produce structured function calls from tool schemas. OpenClaw or Qwen-Agent owns MCP discovery, transport, authorization, `tools/call`, and tool-result delivery. The dataset protocol reference is MCP `2026-07-28`.

## Training representation

The source datasets use semantic Qwen/OpenAI-style messages. QVAC consumes messages-only JSONL with assistant-only loss and Qwen3.5 XML tool calls. Stage 3A/3B used a hardware-constrained compact tool contract; held-out evaluation later showed that the especially narrow Stage-3B continuation did not preserve runtime tool-routing behavior. Stage 3C therefore uses the complete seven-tool inventory and full source JSON schemas in every selected record, while retaining the same assistant-only Qwen3.5 call representation. No chain-of-thought labels and no raw MCP transport packets are trained.

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

The unmodified deployed `Qwen3.5-2B-Q4_K_M` was evaluated directly through llama.cpp `/v1/chat/completions` with `tools` and `tool_choice=auto` on the first 80 frozen test records / 105 assistant decisions.

| Metric | Base Q4 |
|---|---:|
| Tool gate accuracy | 90.48% |
| Tool sequence exact accuracy | 96.67% |
| Exact tool call + arguments | 73.33% |
| Valid argument shape | 97.78% |
| No-tool accuracy | 40.00% |
| False tool-call rate on no-tool cases | 60.00% |
| Mean decision latency | 5.47 s |

The frozen machine-readable baseline is `baselines/base_q4_step2.json`.

## Selected server-native training route

The repository retains the standard Hugging Face PEFT/QLoRA implementation for GPU environments. The deployed Tencent host has no NVIDIA GPU and only about 3.6 GiB RAM, so the selected server route is CPU LoRA against the existing Q4 GGUF with the QVAC llama.cpp fork.

Pinned trainer source:

- repository: `tetherto/qvac-fabric-llm.cpp`
- commit: `657fb2ec6a841f259b4d3d20861864323d5f2737`
- portable trainer on server: `/home/ubuntu/qvac-trainer-step3/llama-finetune-lora`
- base GGUF SHA256: `d6bd9f4175302658c1d704e44858842606652bbe53c56cd542e31ff10d0cfd58`
- Qwen3.5 requirement: `-fa off`
- loss: assistant-only

The base Q4 GGUF is immutable. Training writes separate GGUF LoRA adapters, and a candidate is not promoted merely because its training/validation loss is low.

### Stage 3A — training complete, held-out non-promotion

Stage 3A used rank 4 / alpha 8 LoRA on `attn_q,attn_v`, seed `20260908`, learning rate `1e-5`, one epoch, context 512, and CPU-only execution.

Training evidence:

- train progress: 448 / 448
- train loss: `0.03974`
- train token accuracy: `95.34%`
- validation loss: `0.05669`
- validation token accuracy: `95.83%`
- adapter bytes: `837408`
- adapter SHA256: `db8b9d9509c752941c47261017e52dab0a9590bc1d4f94dc09fc2e061fa0eb27`
- strict inference smoke passed

The later frozen held-out diagnostic showed no exact-call gain: exact call+arguments remained `73.33%`, while tool-gate accuracy fell from `90.48%` to `86.67%` and no-tool accuracy fell from `40.00%` to `26.67%`. Stage 3A is therefore evidence that the QVAC/LoRA path can produce a loadable adapter, but it is **not** a production candidate.

### Stage 3B — training complete, candidate rejected

Stage 3B continued from Stage 3A using a 64-record weakness-targeted curriculum, only two selected tools per record, rank 4 / alpha 8, and learning rate `1e-5`.

Training evidence:

- data SHA256: `0ea19f7143ebcf2de083a4c62ea076050032c49cd6d6e3e6073bf5b0bfc3528c`
- GitHub Actions training run: `34341047994` — success
- final train loss: `0.02288 ± 0.00214`
- final train token accuracy: `96.04 ± 0.34%`
- final validation loss: `0.02203 ± 0.00783`
- final validation token accuracy: `95.38 ± 1.60%`
- adapter bytes: `837408`
- adapter SHA256: `177dafa0114e5f41f7bb4fe518be8a6e2dfbc998ca86687287bb7d2f0dd72502`

The frozen Base-vs-Stage-3B benchmark rejected the adapter despite its low training loss:

| Metric | Base Q4 | Stage 3B 1.00× |
|---|---:|---:|
| Tool gate accuracy | 90.48% | 47.62% |
| Exact tool call + arguments | 73.33% | 10.00% |
| No-tool accuracy | 40.00% | 26.67% |
| API / parse error rate | 0.00% | 0.00% |
| Mean decision latency | 5.47 s | ~4.02 s |

`PROMOTE_RECOMMENDED=false`. The absence of API/parser/load failures means this is a behavioral regression, not benchmark infrastructure failure.

A follow-up `--lora-scaled` sweep on a stratified 32-record subset confirmed excessive update strength rather than a useful full-strength adapter:

| Runtime condition | Tool gate | Exact call+args | No-tool | Mean latency |
|---|---:|---:|---:|---:|
| Base | 90.48% | 75.00% | 33.33% | 6.77 s |
| Stage 3B 0.25× | 90.48% | 75.00% | 33.33% | 5.79 s |
| Stage 3B 0.50× | 90.48% | 72.22% | 33.33% | 5.12 s |
| Stage 3B 0.75× | 90.48% | 55.56% | 33.33% | 4.84 s |
| Stage 3B 1.00× | 45.24% | 5.56% | 33.33% | 3.95 s |

No tested scale produced a strict primary gain over the same base subset, so Stage 3B remains rejected and must not be connected to OpenClaw production routing.

### Stage 3C — fresh balanced recovery candidate

Stage 3C deliberately does **not** continue Stage 3A or Stage 3B. It starts a fresh adapter from the immutable Base Q4 and corrects the largest train/runtime distribution mismatch.

Frozen design:

- 48 selected conversations across all 16 Step-2 task families
- all seven tools retained in every record
- full source JSON schemas retained; no two-tool compaction
- data SHA256: `8987d23fb7be09da32854b6c37f868d8057ea4e7602332b54aa8c4056f6ae14b`
- context: 1024
- batch / microbatch: 8 / 4
- rank / alpha: 4 / 4
- modules: `attn_q,attn_v`
- learning rate: `2e-6`
- epochs: 1
- seed: `20260908`
- initialization: fresh from Base Q4; no `--lora-init`
- output: `/home/ubuntu/local2b-training/step3c/local2b-mcp-step3c-r4a4-qv.gguf`
- no automatic installation

Formal training run `34574746360` was triggered by commit `adf2d5fa768dd9bc2bac44c5993a19ff4185fc7d`; training is currently in progress. After completion, `.github/workflows/local2b-mcp-step3c-eval.yml` will evaluate the adapter on the exact frozen 80-record / 105-decision held-out benchmark before any deployment action.

## Promotion gate

The promotion gate requires:

- zero API/parse errors;
- tool-gate accuracy no worse than Base;
- exact call+arguments no worse than Base;
- no-tool accuracy no worse than Base;
- false-tool-call rate no worse than Base;
- at least one strict primary improvement.

A training loss improvement is not sufficient for promotion. Failed candidates remain archived for diagnosis but are not installed into the OpenClaw production route.

## Generalization guard

Train, validation, and test use disjoint tool names while retaining semantically equivalent JSON schemas. The held-out benchmark therefore rewards reading tool descriptions and schemas rather than memorizing names.

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

### Step 2 — dataset expansion + frozen base benchmark **DONE**
Expand to 19.2k bilingual/distractor/multi-step examples and freeze the pre-LoRA baseline.

### Step 3 — LoRA candidate training **RECOVERY IN PROGRESS**
Stage 3A established the server-native training path; Stage 3B was rejected by held-out behavior testing; Stage 3C is the fresh balanced recovery candidate.

### Step 4 — held-out adapter evaluation **GATED**
Run the exact frozen benchmark for Stage 3C and promote only if it clears the non-regression + strict-gain rule.

### Step 5 — OpenClaw MCP agent integration
Attach the promoted candidate only after the gate passes, then run live end-to-end MCP tasks. Until then, OpenClaw stays on Base Q4.

### Step 6 — deployment A/B
Compare the promoted model against Base on real project tasks and language-quality checks, preserving Base as immediate rollback.

## Key files

- `generate_dataset.py` — deterministic Step-1 generator
- `generate_stage2_dataset.py` — expanded Step-2 curriculum
- `validate_dataset.py` — schema/trajectory/split validator
- `benchmark_tool_use.py` — deterministic direct llama.cpp decision benchmark
- `benchmark_tool_use_trace.py` — detailed per-decision prediction trace
- `prepare_qvac_sft.py` — original Stage-3A QVAC converter
- `prepare_qvac_sft_stage3b.py` — rejected narrow Stage-3B selector
- `prepare_qvac_sft_stage3c.py` — balanced full-tool fresh recovery selector
- `run_server_qvac_lora.sh` — reusable checkpointed server-native LoRA runner
- `baselines/base_q4_step2.json` — frozen pre-LoRA measurement
- `.github/workflows/local2b-mcp-step4-stage3a-diagnostic.yml` — Stage-3A held-out diagnostic
- `.github/workflows/local2b-mcp-step4-ab.yml` — frozen Base vs Stage-3B rejection benchmark
- `.github/workflows/local2b-mcp-step4-scale-sweep.yml` — Stage-3B runtime scale diagnosis
- `.github/workflows/local2b-mcp-step3c-server.yml` — fresh Stage-3C training
- `.github/workflows/local2b-mcp-step3c-eval.yml` — Stage-3C frozen held-out promotion gate
