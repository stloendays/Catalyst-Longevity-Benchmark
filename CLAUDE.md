# Claude Handoff — Catalyst Intelligence Workspace

This repository is an active private project owned by Junbo Tong. Continue from the current `main` branch; do not restart the project from scratch.

## Product direction

The project has evolved from a research benchmark into a **Chinese-first, user-friendly catalyst intelligence workspace** while preserving a rigorous scientific backend.

Do **not** introduce product version numbers into the UI, documentation, filenames, or positioning unless the owner explicitly asks. The owner specifically asked to avoid V1/V2-style framing.

Primary product flow:

```text
01 长期表现分析
    ↓
02 资料分析
    ↓
03 外部数据库
    ↓
04 AI 智能分析
```

Long-term target:

```text
用户上传资料 / Excel / PDF / DOI / 数据库标识
        ↓
资料理解与结构化
        ↓
外部数据库补充
        ↓
Evidence Graph / Evidence Packet
        ↓
科学分析工具
        ↓
AI Analyst
        ↓
Evidence Critic
        ↓
中文结论 + 风险 + 下一步建议
```

## Owner preferences

- Chinese-first product UI and documentation.
- Reduce unnecessary research jargon in the front end.
- Preserve scientific rigor in the backend.
- User should not need to understand censoring, provenance schemas, or ranking mathematics before using the software.
- Prefer direct practical output: which catalyst is better for a target horizon, whether the evidence is sufficient, what data are missing, and what experiment should be run next.
- Never fabricate experimental values, literature data, exact crossover times, or citations.
- Keep source-observed, digitized, model-derived, external-context, and candidate values distinct.
- Do not treat test duration as catalyst lifetime.
- Do not compare catalysts under materially different operating conditions without an explicit method that justifies the comparison.
- Do not make external database data look like experimental longevity ground truth.

## Current UI

Streamlit multipage app:

- `app.py` — 01 长期表现分析
- `pages/1_资料分析.py` — 02 资料分析
- `pages/2_外部数据库检索.py` — 03 外部数据库
- `pages/3_AI智能分析.py` — 04 AI 智能分析
- `src/catlongevity/ui.py` — shared UI design system
- `.streamlit/config.toml` — shared theme

The current UI design intentionally uses a consistent scientific-product style with:

- shared branded header,
- explicit four-step workflow,
- workspace status cards,
- simpler user-facing terminology,
- advanced details pushed behind tabs/expanders,
- strong warning / review / blocked states.

Do not revert to four unrelated Streamlit scripts with inconsistent layout.

## Core software modules

### User data and longevity analysis

- `src/catlongevity/io.py`
  - accepts common Chinese and English column aliases,
  - imports user TOS/performance data,
  - validates inputs conservatively.
- `src/catlongevity/endpoints.py`
  - t95/t90/t80 threshold semantics,
  - exact / interval / left-censored / right-censored / unknown.
- `src/catlongevity/ranking.py`
  - pairwise ranking,
  - crossover brackets,
  - uncertainty-aware ranking,
  - cumulative trapezoid/AUC tools.
- `src/catlongevity/analysis.py`
  - end-to-end deterministic analysis engine.
- `src/catlongevity/condition_matcher.py`
  - checks temperature, GHSV, WHSV, pressure, feed ratio, reaction type when provided,
  - condition mismatch can disable direct ranking.
- `src/catlongevity/friendly.py`
  - translates technical outputs into Chinese user-facing summaries.
- `src/catlongevity/advisor.py`
  - deterministic recommendations,
  - suggests additional measurements / useful time points.
- `src/catlongevity/reporting.py`
  - structured report generation.

### Documents and evidence

- `src/catlongevity/documents.py`
  - PDF/text extraction,
  - conservative DOI / temperature / duration / keyword / conversion candidate detection,
  - candidates are not automatically promoted to trusted measurements.
- `src/catlongevity/evidence.py`
  - Evidence Graph.
- Document candidate principle:
  - a number such as `80.6%` is only a candidate until bound to a specific catalyst, metric, time, condition, and source location.

### External databases

`src/catlongevity/external_databases.py` currently supports:

1. Crossref — DOI and publication metadata.
2. Semantic Scholar — literature discovery and citation context.
3. Catalysis-Hub — computational catalysis context.
4. Materials Project — material properties/structure context.
5. PubChem — compound identity and molecular structure context.

External database records are **context/enrichment**, not longevity truth.

Credentials / runtime environment variables:

```text
OPENAI_API_KEY
OPENAI_MODEL
SEMANTIC_SCHOLAR_API_KEY
MP_API_KEY
```

Never commit API keys or include them in Evidence Packet, reports, exported JSON, logs, examples, or screenshots.

### AI Analyst and Evidence Critic

- `src/catlongevity/ai_analyst.py`
- `pages/3_AI智能分析.py`

Architecture:

```text
Evidence Packet
    ↓
Deterministic precheck
    ↓
AI Analyst
    ↓
Evidence Critic
    ↓
approved / needs_review / blocked
```

Rules:

- AI is not an evidence source.
- Major claims must cite real Evidence IDs.
- Critic must be independent of Analyst.
- Explicit condition mismatch must remain a hard-stop for direct ranking.
- Right-censored lifetime cannot be rewritten as an exact lifetime.
- Sparse observed crossover bracket cannot be rewritten as an exact observed crossover time.
- External database enrichment cannot replace TOS or other appropriate experimental longevity evidence.

## Research backend currently used for validation

The internal scientific validation case remains Ni/Al2O3-based dry reforming of methane (DRM). The repository contains research data/protocols for validating the software logic.

Important confirmed within-paper ranking inversions already established in the broader project:

- Zhou 2015: initial 350 > 700 > 900, later 900 > 700 > 350.
- Zhang 2026: R800 initially > R600, later R600 > R800 under the matched 550 °C comparison.
- Franz 2021: REF initially > 1Fe1Ni, later 1Fe1Ni > REF; initial 1Fe1Ni value is digitized/provisional, so keep caveat.

Luo 2024 is a concordant/no-observed-inversion control through the reported endpoints; do not claim proof that no hidden double crossover exists.

Do not reintroduce N2O negative-control work into this project unless the owner explicitly requests it.

## Current testing discipline

GitHub Actions must remain green.

Current CI includes:

- dependency installation,
- `compileall` for app/pages/src/tests,
- pytest core and user-experience tests,
- Streamlit multipage UI smoke tests.

Do not weaken or remove tests to make a failing feature pass.

Before pushing meaningful changes, verify at minimum:

```bash
python -m compileall -q app.py pages src tests
python -m pytest -q
```

For UI work, keep the Streamlit smoke-test path working.

## Current documentation

- `README.md` — product overview.
- `docs/快速开始.md` — quick start.
- `docs/软件操作说明.md` — user operation manual.
- `docs/中文版智能分析平台架构.md` — architecture.
- `docs/知识产权与软著准备说明.md` — IP/software-copyright preparation context.

## Intellectual-property direction

The owner wants the software to be developed in a form suitable for future Chinese software-copyright registration (软著) and portfolio/commercial use.

Treat this as **software productization and software-copyright preparation**, not as an already-filed patent.

Do not claim that a patent exists or that the software methods are patented. If patent protection is later pursued, patentability, prior art, inventorship, claim drafting, and filing strategy must be handled as a separate workstream.

For the software-copyright-oriented path, preserve:

- coherent product boundaries,
- readable source code,
- stable user workflow,
- operation manual,
- module descriptions,
- input/output definitions,
- error handling,
- UI screenshots when later prepared,
- reproducible installation/launch instructions.

## Immediate next priorities

Continue in this order unless the owner redirects:

1. **Workspace Dashboard**
   - project/home landing page,
   - recent analyses,
   - uploaded documents,
   - catalyst records,
   - evidence count,
   - recent audited AI decisions.
2. **Measurement Binder**
   - bind extracted document candidates to:
     - catalyst,
     - metric,
     - value/unit,
     - time,
     - reaction condition,
     - source locator,
     - evidence class.
   - only bound measurements can enter deterministic analysis.
3. **Document-to-analysis workflow**
   - PDF/table/figure evidence -> Measurement Binder -> Evidence Graph -> deterministic tools -> AI.
4. **Scientific tool calling**
   - allow the AI layer to request deterministic analysis tools rather than manually reasoning about numerical results.
5. **Workspace persistence**
   - save user projects/results locally or in a clear pluggable storage layer without leaking private data.
6. **Report UX**
   - polished Chinese HTML/PDF-style reporting later, with evidence traceability.
7. **Desktop packaging**
   - only after core workflow stabilizes.

## Important known limitation

Current PDF/text extraction can identify candidate values and contextual signals, but it does not yet reliably perform full automatic figure digitization and catalyst/curve identity binding. Do not pretend this capability is complete.

The next major technical value is therefore the Measurement Binder, not adding more external databases.

## Repository/privacy

This repository is private. Do not expose unpublished research data or make the repository public without explicit owner authorization.

## Collaboration rule

When changing scientific semantics, do not silently rewrite frozen research definitions. Product-facing wording may be simplified, but backend evidence/censoring/provenance logic must remain explicit and testable.
