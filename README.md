# 催化剂智能分析平台

**Catalyst Intelligence Workspace** 是一个中文版优先的催化剂资料、长期表现与智能决策分析工具。

用户不需要先把所有资料整理成科研标准格式。平台可以接收：

- CSV / Excel 实验数据；
- PDF 论文、报告和文本资料；
- DOI；
- 外部数据库检索结果；
- 用户提出的催化剂选择或下一步实验问题。

系统将这些信息组织成可追溯证据，再进行长期表现分析、条件可比性检查、AI 综合和独立 Evidence Critic 审查。

## 用户工作流

```text
用户数据 / PDF / DOI / 外部数据库
              ↓
        Evidence Packet
              ↓
实验条件守门 + 长期表现分析
              ↓
          AI Analyst
              ↓
       Evidence Critic
              ↓
通过 / 需复核 / 阻止
              ↓
中文结论 + 风险 + 下一步建议
```

## 1. 长期表现分析

最简单的数据只需要三列：

| 催化剂 | 时间 | 性能 |
|---|---:|---:|
| Catalyst A | 0 | 82 |
| Catalyst A | 20 | 70 |
| Catalyst B | 0 | 76 |
| Catalyst B | 20 | 72 |

系统可以回答：谁起点高、谁保持得更久、是否发生反超、t95/t90/t80 是否真的被测试到，以及下一次最值得增加哪些时间点。

如果提供 **温度、GHSV/WHSV、压力、进料比、反应类型**，系统会先检查实验条件是否可比。存在明确条件不匹配时，相关催化剂对的直接排名会被自动禁用。

## 2. 资料分析

`资料分析` 页面支持 PDF / TXT / Markdown / CSV / TSV。

当前保守提取：

- DOI；
- 温度；
- 测试时长；
- CH4 转化率候选值；
- stability / deactivation / coking / sintering 等证据词；
- 原文证据片段。

孤立的数值不会直接变成实验事实。候选值只有绑定到具体 **催化剂 + 时间 + 条件** 后，才允许进入排名计算。

资料页生成的 Evidence Graph 会传入 AI 工作区，但保持 `candidate_requires_condition_binding` 等证据状态。

## 3. 外部数据库 Hub

当前已接入五类来源：

- **Crossref**：DOI、标题、作者、期刊、年份等论文身份元数据；
- **Semantic Scholar**：相关论文、引用/参考数量、摘要和开放获取入口；
- **Catalysis-Hub**：计算催化反应能、活化能和化学组成；
- **Materials Project**：材料结构与性质背景，如相稳定性、energy above hull、band gap、density；
- **PubChem**：化合物 CID、分子式、分子量、SMILES、InChI / InChIKey。

数据库结果可以由用户加入 **AI 证据篮**。这些结果统一标记为 `external_context`，不能自动充当催化剂长期稳定性真值。

### API Key

- Crossref：无需 key；
- PubChem：无需 key；
- Semantic Scholar：可无 key 尝试，支持用户自己的 API key；
- Materials Project：需要用户自己的 API key；
- OpenAI AI Analyst：需要用户自己的 OpenAI API key。

密钥只从运行时界面或环境变量读取，不写入仓库、Evidence Packet 或下载报告。

支持环境变量：

```text
OPENAI_API_KEY
OPENAI_MODEL
SEMANTIC_SCHOLAR_API_KEY
MP_API_KEY
```

## 4. AI Analyst + Evidence Critic

AI 不是独立证据源。

平台先将确定性分析结果、文档 Evidence Graph 和用户选择的外部数据库记录构造成 **Evidence Packet**，每一项都有稳定 Evidence ID。

`AI Analyst` 必须：

- 只依据 Evidence Packet 回答；
- 每条主要 claim 引用 Evidence ID；
- 不补造实验值；
- 不把稀疏点写成精确 crossover time；
- 不把外部材料/计算数据库直接当 longevity 证据。

随后由独立的 `Evidence Critic` 检查：

- 实验条件是否匹配；
- evidence ID 是否真实存在；
- source-observed / digitized / derived / external 是否混淆；
- censoring 是否被错误解释成精确寿命；
- 是否发生过度外推；
- 关键结论是否缺证据。

最终状态只有三类：

```text
approved      可作为当前证据下的辅助决策建议
needs_review  需要修改/补证据后再用于决策
blocked       当前证据不允许形成强决策结论
```

## 5. Evidence Graph / Evidence Packet

```text
用户资料
 ├─ DOI
 ├─ 实验条件
 ├─ 数值候选
 └─ 原文片段
       ↓
外部身份/背景数据库
       ↓
结构化 TOS 轨迹
       ↓
长期表现与条件审计
       ↓
Evidence Packet（稳定 Evidence IDs）
       ↓
AI Analyst → Evidence Critic
```

最终目标是让每个重要建议都能回答：

> 这个结论来自哪些资料、哪些观测值、什么实验条件，以及哪些部分是模型/计算得到的？

## 安装与启动

```bash
python -m pip install -r requirements-ui.txt
python launch.py
```

Windows 也可以双击：

```text
首次安装_Windows.bat
启动软件_Windows.bat
```

左侧页面：

```text
长期表现分析
资料分析
外部数据库检索
AI 智能分析
```

## 软件结构

```text
Catalyst-Longevity-Benchmark/
├── app.py
├── pages/
│   ├── 1_资料分析.py
│   ├── 2_外部数据库检索.py
│   └── 3_AI智能分析.py
├── src/catlongevity/
│   ├── io.py
│   ├── endpoints.py
│   ├── ranking.py
│   ├── analysis.py
│   ├── condition_matcher.py
│   ├── advisor.py
│   ├── documents.py
│   ├── evidence.py
│   ├── external_databases.py
│   ├── ai_analyst.py
│   └── reporting.py
├── data/
├── protocols/
├── docs/
└── tests/
```

## 后台强制规则

用户友好界面不会削弱底层约束：

- 不虚构缺失实验点；
- 测试 100 h 不自动等于寿命 100 h；
- 稀疏观测不生成假的精确反超时间；
- 条件不匹配会阻止直接排名；
- 文档候选数字必须先绑定催化剂/时间/条件；
- 外部数据库只作背景 enrichment；
- source-observed、digitized、model-derived、external-context 始终分层；
- AI 输出必须再次通过 Evidence Critic。

内部研究数据仍以 **Ni/Al2O3-based dry reforming of methane (DRM)** 为主要验证案例，但软件工作流面向更广泛的催化剂稳定性和性能随时间变化问题。
