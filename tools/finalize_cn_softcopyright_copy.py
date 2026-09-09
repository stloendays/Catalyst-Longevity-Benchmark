from pathlib import Path


def apply(path, pairs):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    for old, new in pairs:
        text = text.replace(old, new)
    p.write_text(text, encoding="utf-8")


apply("native/qt/src/mainwindow.cpp", [
    ('''    const QStringList labels = {
        QStringLiteral("项目"),
        QStringLiteral("总览"),
        QStringLiteral("数据"),
        QStringLiteral("资料分析"),
        QStringLiteral("寿命分析"),
        QStringLiteral("AI 工作区"),
        QStringLiteral("设置")
    };''',
     '''    const QStringList labels = {
        QStringLiteral("项目"),
        QStringLiteral("首页"),
        QStringLiteral("数据"),
        QStringLiteral("资料"),
        QStringLiteral("分析"),
        QStringLiteral("AI 助手"),
        QStringLiteral("设置")
    };'''),
    ('QStringLiteral("项目文件使用本地 SQLite 保存实验记录、实验条件和资料证据候选。关闭软件后可以直接重新打开 .clrproj 继续分析。")',
     'QStringLiteral("项目文件在本地保存实验数据、实验条件和整理后的资料。关闭软件后可直接重新打开 .clrproj 继续分析。")'),
    ('QStringLiteral(".clrproj 内部为 SQLite 数据库。证据候选的来源、摘要、绑定催化剂、绑定时间和人工复核状态会随项目一起保存，但不会自动改写实验观测数据。")',
     'QStringLiteral(".clrproj 项目会保存实验记录、资料来源、关联的催化剂和时间以及人工确认状态，不会自动修改原始实验数据。")'),
    ('QStringLiteral("条件守门")', 'QStringLiteral("条件检查")'),
    ('QStringLiteral("已阻止")', 'QStringLiteral("暂不比较")'),
])

apply("native/qt/src/evidencepage.cpp", [
    ('QStringLiteral("资料分析与 Evidence Packet")', 'QStringLiteral("资料")'),
    ('QStringLiteral("项目证据候选、绑定与复核")', 'QStringLiteral("资料整理")'),
    ('QStringLiteral("Evidence Packet · AI 输入边界")', 'QStringLiteral("AI 可用资料")'),
])

apply("native/qt/src/reportexporter.cpp", [
    ('QStringLiteral("<h2>实验条件守门</h2>")', 'QStringLiteral("<h2>实验条件检查</h2>")'),
    ('<th>条件守门</th>', '<th>条件检查</th>'),
    ('QStringLiteral("<h2>Evidence Packet</h2>")', 'QStringLiteral("<h2>AI 可用资料</h2>")'),
    ('Packet guardrails：', '资料使用规则：'),
    ('<b>Evidence Packet 语义：</b>', '<b>AI 可用资料说明：</b>'),
    ('只有完成催化剂、时间和人工条件复核的条目进入 AI 上下文。Packet 为 context-only，不得覆盖实验观测、寿命阈值或直接排名。',
     '只有已关联催化剂和时间、并经人工确认的资料才会提供给 AI；这些资料不会覆盖实验观测、寿命阈值或直接比较结果。'),
    ('QStringLiteral("<h2>资料证据审计附录</h2>")', 'QStringLiteral("<h2>资料记录附录</h2>")'),
    ('<th>证据候选</th><th>未绑定</th><th>已绑定待复核</th><th>条件已人工复核</th>',
     '<th>资料条目</th><th>未关联</th><th>已关联待确认</th><th>已确认</th>'),
    ('“条件已人工复核”只表示用户完成了资料上下文核对。', '“已确认”表示用户已经核对该条资料的关键实验条件。'),
    ('<b>证据语义：</b>', '<b>结果说明：</b>'),
])

print("Chinese screenshot/report copy finalized.")
