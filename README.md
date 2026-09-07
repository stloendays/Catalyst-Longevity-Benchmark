# 催化剂智能分析平台

**Catalyst Intelligence Workspace** 是一个以中文界面为优先的催化剂长期表现与资料分析工具。

目标不是要求用户先把所有资料整理成科研标准数据，而是让用户可以直接提供：

- CSV / Excel 实验数据；
- PDF 论文或报告；
- 文本资料；
- DOI；
- 外部数据库检索条件；

然后由系统完成资料识别、证据整理、长期表现分析、排名变化判断和下一步建议。

## 现在可以做什么

### 1. 长期表现分析

上传催化剂随时间变化的数据后，可以直接回答：

- 谁的初始性能最高？
- 谁保持得更久？
- 初始领先者后来有没有被反超？
- 95%、90%、80% 保持阈值是否真的被测试到？
- 当前测试时长是否足够支持寿命结论？
- 下一次实验应该优先增加哪些时间点？

最简单的数据只需要三列：

| 催化剂 | 时间 | 性能 |
|---|---:|---:|
| Catalyst A | 0 | 82 |
| Catalyst A | 20 | 70 |
| Catalyst A | 50 | 58 |
| Catalyst B | 0 | 76 |
| Catalyst B | 20 | 72 |
| Catalyst B | 50 | 69 |

### 2. 资料分析

左侧进入 **资料分析** 页面，可以上传：

- PDF；
- TXT；
- Markdown；
- CSV / TSV 文本资料。

系统会保守提取：

- DOI；
- 温度；
- 测试时长；
- CH4 转化率候选值；
- stability / deactivation / coking / sintering 等证据词；
- 可定位的原文证据片段。

候选数值不会自动进入排名。只有在绑定到具体 **催化剂 + 时间 + 条件** 后，才可以进入长期表现分析。

### 3. 外部数据库检索

当前已经接入：

- **Crossref**：DOI 校验、论文标题、作者、期刊、年份等文献元数据；
- **Catalysis-Hub**：计算催化反应能、活化能和体系信息。

外部数据库只是补充证据层，不会替代用户自己的实验数据或论文中的 TOS 数据。

后续计划继续接入：

- Semantic Scholar；
- Materials Project；
- PubChem。

## 智能建议能力

系统现在不只输出计算结果，还会根据现有证据给出操作建议，例如：

```text
A 初始性能高于 B。
B 在 20–50 h 之间反超 A。
如果目标运行时间超过 50 h，现有观测更支持 B。
如果需要更准确确定选择边界，建议在 35 h 左右增加观测，并在 20–50 h 之间加密测试。
```

建议引擎会主动检查：

- 是否发生排名反转；
- 是否缺少共同时间点；
- 是否测试时间仍不足以定义寿命；
- 是否缺少数据来源标签；
- 是否应该增加对照样品；
- 哪个时间区间最值得继续测试。

## Evidence Graph｜证据图谱

平台开始采用统一的证据链：

```text
用户上传资料
    ↓
DOI / 条件 / 数值候选 / 原文片段
    ↓
外部元数据校验
    ↓
结构化实验轨迹
    ↓
长期表现分析
    ↓
结论
    ↓
建议
```

每一个重要结论最终都应该能够回答：

> 这个结论来自哪篇资料、哪个条件、哪些观测值，以及哪些部分是计算得到的？

当前资料页已经可以下载 `证据图谱.json`。

## 安装与启动

第一次使用：

```bash
python -m pip install -r requirements-ui.txt
```

启动：

```bash
python launch.py
```

Windows 用户也可以使用：

```text
首次安装_Windows.bat
启动软件_Windows.bat
```

启动后，Streamlit 左侧会显示多个功能页面：

```text
长期表现分析
资料分析
外部数据库检索
```

## 支持的输入列名

系统兼容常见中英文列名，例如：

- 催化剂：`catalyst_id`、`catalyst`、`sample`、`催化剂`、`样品`
- 时间：`time_h`、`time`、`TOS`、`时间`、`运行时间`
- 性能：`performance`、`value`、`conversion`、`activity`、`性能`、`转化率`、`活性`

如果存在误差范围，可以额外提供 `lower / upper`。

数据模板：[`examples/用户数据模板.csv`](examples/用户数据模板.csv)

## 软件结构

```text
Catalyst-Longevity-Benchmark/
├── app.py                              # 中文主界面：长期表现分析
├── pages/
│   ├── 1_资料分析.py                   # PDF/文本资料入口
│   └── 2_外部数据库检索.py             # Crossref / Catalysis-Hub
├── launch.py                           # 一键启动
├── requirements-ui.txt
├── src/catlongevity/
│   ├── io.py                           # 数据导入与校验
│   ├── endpoints.py                    # 保持阈值/寿命逻辑
│   ├── ranking.py                      # 排名与反超分析
│   ├── analysis.py                     # 长期表现分析引擎
│   ├── friendly.py                     # 中文结果翻译
│   ├── advisor.py                      # 决策与下一步建议
│   ├── documents.py                    # PDF/文本资料解析
│   ├── external_databases.py           # 外部数据库连接器
│   ├── evidence.py                     # Evidence Graph
│   └── reporting.py                    # 报告生成
├── data/                               # 研究证据数据库
├── protocols/                          # 后台严格规则
├── docs/                               # 用户说明与架构文档
└── tests/                              # 自动测试
```

## 后台保留的严谨规则

中文版和用户友好界面不会削弱底层判断：

- 不虚构缺失实验点；
- 测试 100 h 不自动等于寿命 100 h；
- 稀疏观测不会生成假的精确反超时间；
- 不确定结果仍然标记为不确定；
- 外部数据库结果与用户数据分层保存；
- 文本中孤立的转化率数字必须绑定到催化剂、时间和条件后才能进入比较；
- source-observed、digitized、external database、derived calculation 保持区分。

## 当前应用重点

内部研究数据目前仍以 **Ni/Al2O3-based dry reforming of methane (DRM)** 为主要案例，但软件界面本身面向更通用的“性能随时间变化”问题。

最终目标是形成：

```text
资料 / 实验数据 / DOI / 外部数据库
              ↓
        统一证据工作区
              ↓
       催化剂分析引擎
              ↓
     结论 + 风险 + 下一步建议
```
