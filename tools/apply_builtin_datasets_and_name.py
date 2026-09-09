from pathlib import Path


def replace(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if old not in text:
        raise RuntimeError(f"pattern not found in {path}: {old[:80]!r}")
    p.write_text(text.replace(old, new), encoding="utf-8")


replace(
    "native/qt/src/mainwindow.cpp",
    '#include "analysisengine.h"\n',
    '#include "analysisengine.h"\n#include "builtindatasets.h"\n',
)
replace(
    "native/qt/src/mainwindow.cpp",
    '#include <QHeaderView>\n',
    '#include <QHeaderView>\n#include <QInputDialog>\n',
)
replace(
    "native/qt/src/mainwindow.cpp",
    'auto* demoButton = new QPushButton(QStringLiteral("载入示例"));',
    'auto* demoButton = new QPushButton(QStringLiteral("内置数据集"));',
)
replace(
    "native/qt/src/mainwindow.cpp",
    '''    auto* importButton = new QPushButton(QStringLiteral("选择 CSV / Excel"));
    importButton->setObjectName(QStringLiteral("primaryButton"));
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importCsv);
    top->addWidget(importButton);''',
    '''    auto* builtInButton = new QPushButton(QStringLiteral("内置数据集"));
    builtInButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(builtInButton, &QPushButton::clicked, this, &MainWindow::loadDemo);
    auto* importButton = new QPushButton(QStringLiteral("选择 CSV / Excel"));
    importButton->setObjectName(QStringLiteral("primaryButton"));
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importCsv);
    top->addWidget(builtInButton);
    top->addWidget(importButton);''',
)
replace(
    "native/qt/src/mainwindow.cpp",
    '''void MainWindow::loadDemo() {
    setRecords(CsvReader::demoData(), QStringLiteral("内置示例数据"), true);
    setStatus(QStringLiteral("已载入示例数据。"));
}''',
    '''void MainWindow::loadDemo() {
    const auto datasets = BuiltInDatasets::all();
    if (datasets.isEmpty()) {
        setStatus(QStringLiteral("当前没有可用的内置数据集。"), true);
        return;
    }

    QStringList choices;
    choices.reserve(datasets.size());
    for (const auto& dataset : datasets) {
        choices.append(QStringLiteral("%1 · %2").arg(dataset.name, dataset.scenario));
    }

    bool accepted = false;
    const QString selected = QInputDialog::getItem(
        this,
        QStringLiteral("选择内置数据集"),
        QStringLiteral("请选择用于分析或功能演示的数据集："),
        choices,
        0,
        false,
        &accepted);
    if (!accepted || selected.isEmpty()) return;

    const int index = choices.indexOf(selected);
    if (index < 0 || index >= datasets.size()) return;
    const auto& dataset = datasets[index];
    setRecords(
        dataset.records,
        QStringLiteral("内置数据集 · %1").arg(dataset.name),
        true);
    setStatus(QStringLiteral("已载入“%1”：%2 内置数据用于功能演示和流程验证，不作为真实实验结论。")
                  .arg(dataset.name, dataset.description));
}''',
)
replace(
    "native/qt/src/mainwindow.cpp",
    'QStringLiteral("催化剂寿命分析与实验决策软件 V1.0")',
    'QStringLiteral("催化剂寿命数据分析与实验辅助系统")',
)
replace(
    "native/qt/src/mainwindow.cpp",
    'QStringLiteral("V1.0 · Windows 原生桌面版 · C++20 + Qt 6 + SQLite")',
    'QStringLiteral("Windows 原生桌面应用 · C++20 + Qt 6 + SQLite")',
)

replace(
    "native/qt/src/main.cpp",
    '#include "analysisengine.h"\n',
    '#include "analysisengine.h"\n#include "builtindatasets.h"\n',
)
replace(
    "native/qt/src/main.cpp",
    '催化剂寿命分析与实验决策软件 V1.0',
    '催化剂寿命数据分析与实验辅助系统',
)
replace(
    "native/qt/src/main.cpp",
    '''    } else if (text == QStringLiteral("载入示例数据") || text == QStringLiteral("示例数据")) {
        *icon = UiIcon::Demo;
        *tooltip = QStringLiteral("载入内置示例数据快速体验");''',
    '''    } else if (text == QStringLiteral("载入示例数据") || text == QStringLiteral("示例数据") || text == QStringLiteral("内置数据集")) {
        *icon = UiIcon::Demo;
        *tooltip = QStringLiteral("选择内置数据集进行分析或功能演示");''',
)
replace(
    "native/qt/src/main.cpp",
    '''        const auto records = catalyst::CsvReader::demoData();
        const auto result = catalyst::AnalysisEngine::analyze(records);''',
    '''        const auto builtInDatasets = catalyst::BuiltInDatasets::all();
        if (builtInDatasets.size() != 5) return 30;
        for (const auto& dataset : builtInDatasets) {
            if (dataset.name.trimmed().isEmpty() || dataset.records.isEmpty()) return 31;
        }

        const auto records = catalyst::CsvReader::demoData();
        const auto result = catalyst::AnalysisEngine::analyze(records);''',
)

replace(
    "native/qt/src/reportexporter.cpp",
    '催化剂寿命分析与实验决策软件 V1.0',
    '催化剂寿命数据分析与实验辅助系统',
)
replace(
    "native/qt/src/reportexporter.cpp",
    '催化剂寿命与实验决策分析报告',
    '催化剂寿命数据分析与实验辅助报告',
)

print("Built-in datasets and copyright-facing product name applied.")
