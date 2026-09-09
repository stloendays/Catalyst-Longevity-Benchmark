from pathlib import Path


def replace_once(text, old, new, label):
    if old not in text:
        raise RuntimeError(f"missing pattern: {label}")
    return text.replace(old, new, 1)


# CMake: compile the new application-value engine.
p = Path("native/qt/CMakeLists.txt")
text = p.read_text(encoding="utf-8")
text = replace_once(
    text,
    "    src/reportexporter.cpp\n",
    "    src/reportexporter.cpp\n    src/researchadvisor.h\n    src/researchadvisor.cpp\n",
    "CMake researchadvisor",
)
p.write_text(text, encoding="utf-8")

# MainWindow header: add visible data-health / experiment-planning / pair-comparison widgets.
p = Path("native/qt/src/mainwindow.h")
text = p.read_text(encoding="utf-8")
text = replace_once(
    text,
    "    void refreshDecisionOverview();\n",
    "    void refreshDecisionOverview();\n    void refreshResearchSupportViews();\n",
    "refreshResearchSupportViews declaration",
)
text = replace_once(
    text,
    "    QTableWidget* thresholdTable_ = nullptr;\n",
    "    QTableWidget* thresholdTable_ = nullptr;\n"
    "    QLabel* dataCheckStatus_ = nullptr;\n"
    "    QLabel* dataCheckScore_ = nullptr;\n"
    "    QTableWidget* dataCheckTable_ = nullptr;\n"
    "    QTableWidget* comparisonTable_ = nullptr;\n"
    "    QTableWidget* adviceTable_ = nullptr;\n",
    "research support widget members",
)
p.write_text(text, encoding="utf-8")

# MainWindow UI.
p = Path("native/qt/src/mainwindow.cpp")
text = p.read_text(encoding="utf-8")
text = replace_once(
    text,
    '#include "reportexporter.h"\n',
    '#include "reportexporter.h"\n#include "researchadvisor.h"\n',
    "researchadvisor include",
)
text = replace_once(
    text,
    "#include <QTableWidget>\n",
    "#include <QTableWidget>\n#include <QTabWidget>\n",
    "QTabWidget include",
)

# Style analysis tabs like a native modern desktop application.
text = replace_once(
    text,
    '        QTableWidget::item:selected { background:#EDEDEF; color:#111111; }\n',
    '        QTableWidget::item:selected { background:#EDEDEF; color:#111111; }\n'
    '        QTabWidget::pane { background:#FFFFFF; border:1px solid #E4E4E7; border-radius:12px; top:-1px; }\n'
    '        QTabBar::tab { background:#F4F4F5; color:#71717A; border:1px solid #E4E4E7; padding:8px 18px; margin-right:4px; border-top-left-radius:8px; border-top-right-radius:8px; }\n'
    '        QTabBar::tab:hover { background:#ECECEE; color:#27272A; }\n'
    '        QTabBar::tab:selected { background:#FFFFFF; color:#111111; border-bottom-color:#FFFFFF; font-weight:650; }\n',
    "tab stylesheet",
)

# Make remaining page subtitles natural Chinese.
text = text.replace(
    'QStringLiteral("导入 CSV 或 Excel 后，直接计算保持率、删失感知寿命阈值与实验条件守门结果。")',
    'QStringLiteral("查看催化剂长期表现、寿命指标和实验条件检查结果。")',
)
text = text.replace(
    'QStringLiteral("T95 / T90 / T80 保留离散观测的删失语义；直接跨催化剂结论同时受实验条件守门约束。")',
    'QStringLiteral("查看寿命区间、同时间对比和下一步实验建议。比较前会自动检查实验条件。")',
)
text = text.replace(
    'QStringLiteral("AI 只在受控证据边界内工作。先构建证据包，再完成分析、证据审查与可追溯输出；任何阶段都不会自动改写原始实验观测。")',
    'QStringLiteral("AI 只使用你已经确认的资料进行分析，不会修改原始实验数据。")',
)
text = text.replace(
    'QStringLiteral("未绑定候选、条件未复核条目和外部背景记录不会直接成为寿命结论。最终输出必须保留证据来源与审查状态。")',
    'QStringLiteral("未关联或未确认的资料不会提供给 AI。分析结果会保留资料来源和确认状态，方便后续核对。")',
)

# Data page: add an actual data-health module before the raw table.
old_data = '''    sourceLayout->addWidget(sourceLabel_);
    layout->addWidget(sourceFrame);

    rawTable_ = new QTableWidget(0, 9);'''
new_data = '''    sourceLayout->addWidget(sourceLabel_);
    layout->addWidget(sourceFrame);

    auto* checkFrame = new QFrame;
    checkFrame->setObjectName(QStringLiteral("gptSurface"));
    auto* checkLayout = new QVBoxLayout(checkFrame);
    checkLayout->setContentsMargins(18, 14, 18, 14);
    checkLayout->setSpacing(10);
    auto* checkTop = new QHBoxLayout;
    auto* checkTitle = new QLabel(QStringLiteral("数据检查"));
    checkTitle->setObjectName(QStringLiteral("sectionTitle"));
    dataCheckStatus_ = new QLabel(QStringLiteral("暂无数据"));
    dataCheckStatus_->setObjectName(QStringLiteral("statusNeutral"));
    dataCheckScore_ = muted(QStringLiteral("完整度评分：—"));
    checkTop->addWidget(checkTitle);
    checkTop->addWidget(dataCheckStatus_);
    checkTop->addStretch();
    checkTop->addWidget(dataCheckScore_);
    checkLayout->addLayout(checkTop);
    checkLayout->addWidget(muted(QStringLiteral("自动检查重复时间点、异常数值、实验条件缺失和不同催化剂之间的条件差异。")));
    dataCheckTable_ = new QTableWidget(0, 4);
    dataCheckTable_->setHorizontalHeaderLabels({
        QStringLiteral("状态"), QStringLiteral("范围"), QStringLiteral("发现"), QStringLiteral("建议")});
    configureTable(dataCheckTable_);
    dataCheckTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    dataCheckTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    dataCheckTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    dataCheckTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    dataCheckTable_->setMaximumHeight(210);
    checkLayout->addWidget(dataCheckTable_);
    layout->addWidget(checkFrame);

    rawTable_ = new QTableWidget(0, 9);'''
text = replace_once(text, old_data, new_data, "data check UI")

# Analysis page: replace the single lifetime table with three useful tabs.
old_analysis = '''    thresholdTable_ = new QTableWidget(0, 7);
    thresholdTable_->setHorizontalHeaderLabels({
        QStringLiteral("催化剂"), QStringLiteral("保持率"), QStringLiteral("T95"),
        QStringLiteral("T90"), QStringLiteral("T80"), QStringLiteral("初始值"), QStringLiteral("最新值")});
    configureTable(thresholdTable_);
    layout->addWidget(thresholdTable_, 1);

    auto* note = new QFrame;'''
new_analysis = '''    auto* analysisTabs = new QTabWidget;

    auto* lifetimeTab = new QWidget;
    auto* lifetimeLayout = new QVBoxLayout(lifetimeTab);
    lifetimeLayout->setContentsMargins(12, 12, 12, 12);
    thresholdTable_ = new QTableWidget(0, 7);
    thresholdTable_->setHorizontalHeaderLabels({
        QStringLiteral("催化剂"), QStringLiteral("保持率"), QStringLiteral("T95"),
        QStringLiteral("T90"), QStringLiteral("T80"), QStringLiteral("初始值"), QStringLiteral("最新值")});
    configureTable(thresholdTable_);
    lifetimeLayout->addWidget(thresholdTable_);
    analysisTabs->addTab(lifetimeTab, QStringLiteral("寿命指标"));

    auto* compareTab = new QWidget;
    auto* compareLayout = new QVBoxLayout(compareTab);
    compareLayout->setContentsMargins(12, 12, 12, 12);
    compareLayout->addWidget(muted(QStringLiteral("按两个催化剂最近的共同观测时间进行比较；实验条件不一致时不会给出直接领先结论。")));
    comparisonTable_ = new QTableWidget(0, 8);
    comparisonTable_->setHorizontalHeaderLabels({
        QStringLiteral("催化剂 A"), QStringLiteral("催化剂 B"), QStringLiteral("共同时间(h)"),
        QStringLiteral("A 性能"), QStringLiteral("B 性能"), QStringLiteral("差值"),
        QStringLiteral("状态"), QStringLiteral("结果")});
    configureTable(comparisonTable_);
    compareLayout->addWidget(comparisonTable_);
    analysisTabs->addTab(compareTab, QStringLiteral("同时间对比"));

    auto* adviceTab = new QWidget;
    auto* adviceLayout = new QVBoxLayout(adviceTab);
    adviceLayout->setContentsMargins(12, 12, 12, 12);
    adviceLayout->addWidget(muted(QStringLiteral("根据当前观测点、T90 区间和实验条件自动生成下一轮实验建议。建议为规则计算结果，可直接用于实验计划讨论。")));
    adviceTable_ = new QTableWidget(0, 5);
    adviceTable_->setHorizontalHeaderLabels({
        QStringLiteral("优先级"), QStringLiteral("催化剂"), QStringLiteral("建议"),
        QStringLiteral("原因"), QStringLiteral("下一步")});
    configureTable(adviceTable_);
    adviceLayout->addWidget(adviceTable_);
    analysisTabs->addTab(adviceTab, QStringLiteral("实验建议"));

    layout->addWidget(analysisTabs, 1);

    auto* note = new QFrame;'''
text = replace_once(text, old_analysis, new_analysis, "analysis application tabs")

# Refresh application-value modules after every analysis/data update.
text = replace_once(
    text,
    "    refreshDecisionOverview();\n}\n\nvoid MainWindow::refreshDecisionOverview() {",
    "    refreshDecisionOverview();\n    refreshResearchSupportViews();\n}\n\nvoid MainWindow::refreshDecisionOverview() {",
    "research support refresh hook",
)

# Insert deterministic visible results before updateProjectUi.
marker = "void MainWindow::updateProjectUi() {"
research_fn = r'''void MainWindow::refreshResearchSupportViews() {
    const DataCheckResult check = ResearchAdvisor::checkData(records_, analysis_);
    if (dataCheckStatus_) {
        QString style = QStringLiteral("statusGood");
        if (records_.isEmpty()) style = QStringLiteral("statusNeutral");
        else if (check.errorCount > 0) style = QStringLiteral("statusBad");
        else if (check.warningCount > 0) style = QStringLiteral("statusWarn");
        setStatusChip(dataCheckStatus_, check.statusText(), style);
    }
    if (dataCheckScore_) {
        dataCheckScore_->setText(records_.isEmpty()
            ? QStringLiteral("完整度评分：—")
            : QStringLiteral("完整度评分：%1 / 100 · %2 个需处理 · %3 个建议")
                .arg(check.score).arg(check.errorCount).arg(check.warningCount));
    }
    if (dataCheckTable_) {
        dataCheckTable_->setRowCount(check.items.size());
        for (qsizetype row = 0; row < check.items.size(); ++row) {
            const auto& item = check.items[row];
            auto* level = readOnlyItem(ResearchAdvisor::checkLevelText(item.level));
            if (item.level == CheckLevel::Error) {
                level->setForeground(QColor(QStringLiteral("#991B1B")));
                level->setBackground(QColor(QStringLiteral("#FEF2F2")));
            } else if (item.level == CheckLevel::Warning) {
                level->setForeground(QColor(QStringLiteral("#92400E")));
                level->setBackground(QColor(QStringLiteral("#FFFBEB")));
            } else {
                level->setForeground(QColor(QStringLiteral("#52525B")));
                level->setBackground(QColor(QStringLiteral("#F4F4F5")));
            }
            dataCheckTable_->setItem(row, 0, level);
            dataCheckTable_->setItem(row, 1, readOnlyItem(item.scope));
            dataCheckTable_->setItem(row, 2, readOnlyItem(item.issue));
            dataCheckTable_->setItem(row, 3, readOnlyItem(item.suggestion));
        }
        dataCheckTable_->resizeRowsToContents();
    }

    if (comparisonTable_) {
        const auto comparisons = ResearchAdvisor::pairComparisons(records_, analysis_);
        comparisonTable_->setRowCount(comparisons.size());
        for (qsizetype row = 0; row < comparisons.size(); ++row) {
            const auto& item = comparisons[row];
            comparisonTable_->setItem(row, 0, readOnlyItem(item.catalystA));
            comparisonTable_->setItem(row, 1, readOnlyItem(item.catalystB));
            comparisonTable_->setItem(row, 2, readOnlyItem(item.sharedTimeHours > 0.0
                ? QString::number(item.sharedTimeHours, 'g', 8) : QStringLiteral("—")));
            comparisonTable_->setItem(row, 3, readOnlyItem(item.sharedTimeHours > 0.0
                ? QString::number(item.performanceA, 'g', 8) : QStringLiteral("—")));
            comparisonTable_->setItem(row, 4, readOnlyItem(item.sharedTimeHours > 0.0
                ? QString::number(item.performanceB, 'g', 8) : QStringLiteral("—")));
            comparisonTable_->setItem(row, 5, readOnlyItem(item.sharedTimeHours > 0.0
                ? QString::number(item.absoluteDifference, 'g', 8) : QStringLiteral("—")));
            auto* status = readOnlyItem(item.status);
            if (item.status == QStringLiteral("条件不一致")) {
                status->setForeground(QColor(QStringLiteral("#991B1B")));
                status->setBackground(QColor(QStringLiteral("#FEF2F2")));
            } else if (item.status == QStringLiteral("仅供参考")) {
                status->setForeground(QColor(QStringLiteral("#92400E")));
                status->setBackground(QColor(QStringLiteral("#FFFBEB")));
            } else if (item.status == QStringLiteral("可比较")) {
                status->setForeground(QColor(QStringLiteral("#166534")));
                status->setBackground(QColor(QStringLiteral("#F0FDF4")));
            }
            comparisonTable_->setItem(row, 6, status);
            const QString outcome = item.comparable && !item.leader.isEmpty()
                ? QStringLiteral("%1 当前较高").arg(item.leader)
                : QStringLiteral("暂不判断");
            comparisonTable_->setItem(row, 7, readOnlyItem(outcome));
        }
        comparisonTable_->resizeRowsToContents();
    }

    if (adviceTable_) {
        const auto advice = ResearchAdvisor::experimentAdvice(records_, analysis_);
        adviceTable_->setRowCount(advice.size());
        for (qsizetype row = 0; row < advice.size(); ++row) {
            const auto& item = advice[row];
            auto* priority = readOnlyItem(ResearchAdvisor::advicePriorityText(item.priority));
            if (item.priority == AdvicePriority::High) {
                priority->setForeground(QColor(QStringLiteral("#991B1B")));
                priority->setBackground(QColor(QStringLiteral("#FEF2F2")));
            } else if (item.priority == AdvicePriority::Important) {
                priority->setForeground(QColor(QStringLiteral("#92400E")));
                priority->setBackground(QColor(QStringLiteral("#FFFBEB")));
            } else {
                priority->setForeground(QColor(QStringLiteral("#52525B")));
                priority->setBackground(QColor(QStringLiteral("#F4F4F5")));
            }
            adviceTable_->setItem(row, 0, priority);
            adviceTable_->setItem(row, 1, readOnlyItem(item.catalyst));
            adviceTable_->setItem(row, 2, readOnlyItem(item.action));
            adviceTable_->setItem(row, 3, readOnlyItem(item.reason));
            adviceTable_->setItem(row, 4, readOnlyItem(item.target));
        }
        adviceTable_->resizeRowsToContents();
    }
}

'''
text = replace_once(text, marker, research_fn + marker, "refreshResearchSupportViews implementation")
p.write_text(text, encoding="utf-8")

# Native self-test must cover the new functional engine, not only UI presence.
p = Path("native/qt/src/main.cpp")
text = p.read_text(encoding="utf-8")
text = replace_once(
    text,
    '#include "reportexporter.h"\n',
    '#include "reportexporter.h"\n#include "researchadvisor.h"\n',
    "main researchadvisor include",
)
text = replace_once(
    text,
    '''        if (!QFileInfo::exists(reportPath) || QFileInfo(reportPath).size() <= 0) return 25;

        return 0;''',
    '''        if (!QFileInfo::exists(reportPath) || QFileInfo(reportPath).size() <= 0) return 25;

        const auto dataCheck = catalyst::ResearchAdvisor::checkData(records, result);
        if (dataCheck.score <= 0 || dataCheck.items.isEmpty()) return 26;
        const auto experimentAdvice = catalyst::ResearchAdvisor::experimentAdvice(records, result);
        if (experimentAdvice.isEmpty()) return 27;
        const auto comparisons = catalyst::ResearchAdvisor::pairComparisons(records, result);
        if (comparisons.size() != 1
            || comparisons.front().catalystA.isEmpty()
            || comparisons.front().catalystB.isEmpty()
            || comparisons.front().sharedTimeHours <= 0.0) return 28;

        const auto mismatchComparisons = catalyst::ResearchAdvisor::pairComparisons(mismatched, mismatchResult);
        if (mismatchComparisons.size() != 1
            || mismatchComparisons.front().comparable
            || mismatchComparisons.front().status != QStringLiteral("条件不一致")) return 29;

        return 0;''',
    "research advisor self test",
)
p.write_text(text, encoding="utf-8")

print("Research-value modules integrated into native Qt UI.")
