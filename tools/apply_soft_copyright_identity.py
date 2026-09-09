from pathlib import Path


def apply(path, replacements):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    before = text
    for old, new in replacements:
        text = text.replace(old, new)
    if text == before:
        raise RuntimeError(f"no changes in {path}")
    p.write_text(text, encoding="utf-8")


apply("native/qt/src/mainwindow.cpp", [
    ('QStringLiteral("总览"),\n        QStringLiteral("数据导入"),\n        QStringLiteral("资料分析"),\n        QStringLiteral("寿命分析"),\n        QStringLiteral("AI 工作区")',
     'QStringLiteral("首页"),\n        QStringLiteral("数据"),\n        QStringLiteral("资料"),\n        QStringLiteral("分析"),\n        QStringLiteral("AI 助手")'),
    ('QStringLiteral("Catalyst Longevity Research")', 'QStringLiteral("催化剂寿命分析与实验决策软件 V1.0")'),
    ('QStringLiteral("打开 Catalyst Longevity Research 项目")', 'QStringLiteral("打开催化剂寿命分析项目")'),
    ('QStringLiteral("保存 Catalyst Longevity Research 项目")', 'QStringLiteral("保存催化剂寿命分析项目")'),
    ('QStringLiteral("Catalyst-Longevity-Research.clrproj")', 'QStringLiteral("催化剂寿命分析项目.clrproj")'),
    ('QStringLiteral("Catalyst Longevity 项目 (*.clrproj)")', 'QStringLiteral("催化剂寿命分析项目 (*.clrproj)")'),
    ('QStringLiteral("原生 Windows 桌面版 · C++20 + Qt 6 Widgets + SQLite")', 'QStringLiteral("V1.0 · Windows 原生桌面版 · C++20 + Qt 6 + SQLite")'),
    ('QStringLiteral("项目存储：本地 .clrproj SQLite 文件（实验记录 + 证据候选/复核状态）。")',
     'QStringLiteral("项目存储：本地 .clrproj 文件（实验数据、资料关联和确认状态）。")'),
    ('QStringLiteral("报告输出：原生 PDF 分析报告 + 证据审计附录。")',
     'QStringLiteral("核心功能：数据检查、寿命分析、同时间对比、实验建议、资料整理和 PDF 报告。")'),
    ('"导入 CSV 或 Excel 后，直接计算保持率、删失感知寿命阈值与实验条件守门结果。"',
     '"查看催化剂长期表现、寿命指标和实验条件检查结果。"'),
    ('"T95 / T90 / T80 保留离散观测的删失语义；直接跨催化剂结论同时受实验条件守门约束。"',
     '"查看寿命区间、同时间对比和下一步实验建议。比较前会自动检查实验条件。"'),
    ('"AI 只在受控证据边界内工作。先构建证据包，再完成分析、证据审查与可追溯输出；任何阶段都不会自动改写原始实验观测。"',
     '"AI 只使用你已经确认的资料进行分析，不会修改原始实验数据。"'),
    ('"未绑定候选、条件未复核条目和外部背景记录不会直接成为寿命结论。最终输出必须保留证据来源与审查状态。"',
     '"未关联或未确认的资料不会提供给 AI。分析结果会保留资料来源和确认状态，方便后续核对。"'),
])

apply("native/qt/src/main.cpp", [
    ('app.setApplicationDisplayName(QStringLiteral("催化剂寿命研究工作台"));',
     'app.setApplicationDisplayName(QStringLiteral("催化剂寿命分析与实验决策软件 V1.0"));'),
    ('window.setWindowTitle(QStringLiteral("催化剂寿命研究工作台"));',
     'window.setWindowTitle(QStringLiteral("催化剂寿命分析与实验决策软件 V1.0"));'),
])

apply("native/qt/src/reportexporter.cpp", [
    ('<h1>Catalyst Longevity Research</h1>', '<h1>催化剂寿命分析与实验决策软件 V1.0</h1>'),
    ('<p class=\'meta\'>催化剂长期表现分析报告</p>', '<p class=\'meta\'>催化剂寿命与实验决策分析报告</p>'),
    ('QStringLiteral("Catalyst Longevity Research Analysis Report")', 'QStringLiteral("催化剂寿命与实验决策分析报告")'),
    ('QStringLiteral("Catalyst Longevity Research")', 'QStringLiteral("催化剂寿命分析与实验决策软件 V1.0")'),
])

print("Soft-copyright identity polish applied.")
