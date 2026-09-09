from pathlib import Path

mainwindow = Path('native/qt/src/mainwindow.cpp')
report = Path('native/qt/src/reportexporter.cpp')


def replace(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding='utf-8')
    if old not in text:
        raise SystemExit(f'Missing {label} in {path}')
    path.write_text(text.replace(old, new), encoding='utf-8')

replace(
    mainwindow,
    '    root->addWidget(pages_, 1);\n\n    connect(evidencePage_, &EvidencePage::evidenceChanged, this, [this]() {',
    '    root->addWidget(pages_, 1);\n    pages_->setCurrentIndex(1);\n\n    connect(evidencePage_, &EvidencePage::evidenceChanged, this, [this]() {',
    'default homepage',
)

replace(
    mainwindow,
    '''void setStatusChip(QLabel* label, const QString& text, const QString& objectName) {
    if (!label) return;
    label->setText(text);
    if (label->objectName() != objectName) {''',
    '''void setStatusChip(QLabel* label, const QString& text, const QString& objectName) {
    if (!label) return;
    const bool semantic = objectName == QStringLiteral("statusGood")
        || objectName == QStringLiteral("statusWarn")
        || objectName == QStringLiteral("statusBad");
    label->setText(semantic ? QStringLiteral("●  %1").arg(text) : text);
    if (label->objectName() != objectName) {''',
    'plain status markers',
)

replace(
    mainwindow,
    '''    cardLayout->setContentsMargins(22, 20, 22, 20);
        auto* productName = new QLabel(QStringLiteral("智策"));
    productName->setObjectName(QStringLiteral("sectionTitle"));
    cardLayout->addWidget(productName);
    cardLayout->addWidget(muted(QStringLiteral("催化剂长期稳定性评估与实验决策系统")));
    cardLayout->addWidget(muted(QStringLiteral("Windows 原生桌面应用 · C++20 + Qt 6 + SQLite")));
    cardLayout->addSpacing(10);
    cardLayout->addWidget(new QLabel(QStringLiteral("运行方式：本地桌面窗口，不启动浏览器，不依赖 Streamlit。")));''',
    '''    cardLayout->setContentsMargins(22, 20, 22, 20);
    auto* productName = new QLabel(QStringLiteral("智策"));
    productName->setObjectName(QStringLiteral("pageHeading"));
    cardLayout->addWidget(productName);
    cardLayout->addWidget(muted(QStringLiteral("催化剂长期稳定性评估与实验决策")));
    cardLayout->addSpacing(10);
    cardLayout->addWidget(new QLabel(QStringLiteral("技术架构：C++20 · Qt 6 · SQLite。")));''',
    'settings card cleanup',
)

replace(
    mainwindow,
    '        QStringLiteral("Catalyst Longevity 项目 (*.clrproj);;所有文件 (*.*)"));',
    '        QStringLiteral("智策项目 (*.clrproj);;所有文件 (*.*)"));',
    'open-project file filter',
)
replace(
    mainwindow,
    '        currentProjectPath_.isEmpty() ? QStringLiteral("催化剂寿命分析项目.clrproj") : currentProjectPath_,\n        QStringLiteral("催化剂寿命分析项目 (*.clrproj)"));',
    '        currentProjectPath_.isEmpty() ? QStringLiteral("智策项目.clrproj") : currentProjectPath_,\n        QStringLiteral("智策项目 (*.clrproj)"));',
    'save-as project naming',
)
replace(
    mainwindow,
    '            QStringLiteral("催化剂寿命分析项目.clrproj"),\n            QStringLiteral("催化剂寿命分析项目 (*.clrproj)"));',
    '            QStringLiteral("智策项目.clrproj"),\n            QStringLiteral("智策项目 (*.clrproj)"));',
    'transition save naming',
)
replace(
    mainwindow,
    '        ? QStringLiteral("Catalyst-Longevity-Analysis-Report.pdf")',
    '        ? QStringLiteral("智策-分析报告.pdf")',
    'default PDF report file name',
)

replace(
    report,
    '''    html += QStringLiteral("<h1>催化剂长期稳定性评估与实验决策系统</h1>");
    html += QStringLiteral("<p class='meta'>催化剂寿命数据分析与实验辅助报告</p>");''',
    '''    html += QStringLiteral("<h1>智策</h1>");
    html += QStringLiteral("<p class='meta'>催化剂长期稳定性评估与实验决策报告</p>");''',
    'PDF report identity',
)

print('Final Zhice UI cleanup applied.')
