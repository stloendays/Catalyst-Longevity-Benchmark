from pathlib import Path


def replace_once(path: str, old: str, new: str, label: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if old not in text:
        raise RuntimeError(f"missing pattern for {label} in {path}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# CMake: compile the offline public reference snapshot.
replace_once(
    "native/qt/CMakeLists.txt",
    "    src/builtindatasets.h\n    src/builtindatasets.cpp\n",
    "    src/builtindatasets.h\n    src/builtindatasets.cpp\n    src/referenceknowledge.h\n    src/referenceknowledge.cpp\n",
    "reference knowledge sources",
)

# Main window: add the reference library tab and richer advice columns.
replace_once(
    "native/qt/src/mainwindow.cpp",
    '#include "reportexporter.h"\n#include "researchadvisor.h"\n',
    '#include "reportexporter.h"\n#include "referenceknowledge.h"\n#include "researchadvisor.h"\n',
    "mainwindow reference include",
)

old_advice_tab = '''    auto* adviceTab = new QWidget;
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
'''
new_advice_tab = '''    auto* adviceTab = new QWidget;
    auto* adviceLayout = new QVBoxLayout(adviceTab);
    adviceLayout->setContentsMargins(12, 12, 12, 12);
    adviceLayout->addWidget(muted(QStringLiteral(
        "建议会同时检查当前采样间隔、实验条件完整度、建议跨度和可匹配的公开实验窗口。公开参考只用于校验量级，不会直接替代你的实验条件。")));
    adviceTable_ = new QTableWidget(0, 7);
    adviceTable_->setHorizontalHeaderLabels({
        QStringLiteral("优先级"), QStringLiteral("催化剂"), QStringLiteral("建议"),
        QStringLiteral("可执行性"), QStringLiteral("原因"), QStringLiteral("下一步"),
        QStringLiteral("依据")});
    configureTable(adviceTable_);
    adviceTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    adviceTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    adviceTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    adviceTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    adviceTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    adviceTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    adviceTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    adviceLayout->addWidget(adviceTable_);
    analysisTabs->addTab(adviceTab, QStringLiteral("实验建议"));

    auto* referenceTab = new QWidget;
    auto* referenceLayout = new QVBoxLayout(referenceTab);
    referenceLayout->setContentsMargins(12, 12, 12, 12);
    referenceLayout->setSpacing(10);
    referenceLayout->addWidget(muted(QStringLiteral(
        "内置参考库保存少量公开数据库事实与公开论文实验条件快照，可离线查看。只保存标识符、基础物性和实验窗口等事实信息，不内置论文全文或图表。")));
    referenceMatchSummary_ = muted(QStringLiteral("导入数据后，系统会显示哪些公开实验窗口与当前条件更接近。"));
    referenceLayout->addWidget(referenceMatchSummary_);
    referenceTable_ = new QTableWidget(0, 7);
    referenceTable_->setHorizontalHeaderLabels({
        QStringLiteral("来源"), QStringLiteral("条目"), QStringLiteral("标识符"),
        QStringLiteral("关键信息"), QStringLiteral("用途"), QStringLiteral("来源地址"),
        QStringLiteral("快照日期")});
    configureTable(referenceTable_);
    referenceTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    referenceTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    referenceTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    referenceTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    referenceTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    referenceTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    referenceTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    referenceLayout->addWidget(referenceTable_);
    analysisTabs->addTab(referenceTab, QStringLiteral("参考库"));
'''
replace_once("native/qt/src/mainwindow.cpp", old_advice_tab, new_advice_tab, "analysis advice/reference tabs")

old_advice_refresh = '''    if (adviceTable_) {
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
'''
new_advice_refresh = '''    if (adviceTable_) {
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
            auto* feasibility = readOnlyItem(QStringLiteral("%1 · %2/100")
                .arg(item.feasibility).arg(item.feasibilityScore));
            if (item.feasibilityScore >= 85) {
                feasibility->setForeground(QColor(QStringLiteral("#166534")));
                feasibility->setBackground(QColor(QStringLiteral("#F0FDF4")));
            } else if (item.feasibilityScore >= 70) {
                feasibility->setForeground(QColor(QStringLiteral("#92400E")));
                feasibility->setBackground(QColor(QStringLiteral("#FFFBEB")));
            } else {
                feasibility->setForeground(QColor(QStringLiteral("#991B1B")));
                feasibility->setBackground(QColor(QStringLiteral("#FEF2F2")));
            }
            adviceTable_->setItem(row, 0, priority);
            adviceTable_->setItem(row, 1, readOnlyItem(item.catalyst));
            adviceTable_->setItem(row, 2, readOnlyItem(item.action));
            adviceTable_->setItem(row, 3, feasibility);
            adviceTable_->setItem(row, 4, readOnlyItem(item.reason));
            adviceTable_->setItem(row, 5, readOnlyItem(item.target));
            adviceTable_->setItem(row, 6, readOnlyItem(item.basis));
        }
        adviceTable_->resizeRowsToContents();
    }

    const auto referenceMatches = ReferenceKnowledgeBase::matchExperimentContext(records_);
    if (referenceMatchSummary_) {
        if (records_.isEmpty()) {
            referenceMatchSummary_->setText(QStringLiteral("导入数据后，系统会显示哪些公开实验窗口与当前条件更接近。"));
        } else if (referenceMatches.isEmpty()) {
            referenceMatchSummary_->setText(QStringLiteral(
                "当前数据没有匹配到足够接近的内置公开实验窗口。实验建议将只依据本次数据，不强行套用文献条件。"));
        } else {
            referenceMatchSummary_->setText(QStringLiteral("当前匹配到 %1 个公开实验窗口；最高相关度 %2/100。%3")
                .arg(referenceMatches.size())
                .arg(referenceMatches.front().relevanceScore)
                .arg(ReferenceKnowledgeBase::compactMatchText(referenceMatches, 2)));
        }
    }
    if (referenceTable_) {
        const auto entries = ReferenceKnowledgeBase::entries();
        referenceTable_->setRowCount(entries.size());
        for (qsizetype row = 0; row < entries.size(); ++row) {
            const auto& entry = entries[row];
            referenceTable_->setItem(row, 0, readOnlyItem(entry.source));
            referenceTable_->setItem(row, 1, readOnlyItem(entry.title));
            auto* identifier = readOnlyItem(entry.identifier);
            identifier->setToolTip(entry.sourceUrl);
            referenceTable_->setItem(row, 2, identifier);
            referenceTable_->setItem(row, 3, readOnlyItem(entry.keyFacts));
            referenceTable_->setItem(row, 4, readOnlyItem(entry.use));
            auto* url = readOnlyItem(entry.sourceUrl);
            url->setToolTip(entry.sourceUrl);
            referenceTable_->setItem(row, 5, url);
            referenceTable_->setItem(row, 6, readOnlyItem(entry.snapshotDate));
        }
        referenceTable_->resizeRowsToContents();
    }
'''
replace_once("native/qt/src/mainwindow.cpp", old_advice_refresh, new_advice_refresh, "advice/reference refresh")

# Report: include executability/basis and the offline public reference snapshot.
replace_once(
    "native/qt/src/reportexporter.cpp",
    '#include "evidencepacket.h"\n#include "researchadvisor.h"\n',
    '#include "evidencepacket.h"\n#include "referenceknowledge.h"\n#include "researchadvisor.h"\n',
    "report reference include",
)

old_report_advice = '''    const auto experimentAdvice = ResearchAdvisor::experimentAdvice(records, result);
    html += QStringLiteral("<h2>下一步实验建议</h2>");
    if (experimentAdvice.isEmpty()) {
        html += QStringLiteral("<p>当前数据未触发额外实验建议。</p>");
    } else {
        html += QStringLiteral("<table><tr><th>优先级</th><th>催化剂</th><th>建议</th><th>原因</th><th>下一步</th></tr>");
        for (const auto& advice : experimentAdvice) {
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                .arg(escape(ResearchAdvisor::advicePriorityText(advice.priority)),
                     escape(advice.catalyst), escape(advice.action), escape(advice.reason), escape(advice.target));
        }
        html += QStringLiteral("</table>");
    }
    html += QStringLiteral(
        "<p class='note'><b>实验建议说明：</b>上述建议由当前观测点、T90 状态、采样间隔和实验条件规则自动生成，"
        "用于辅助下一轮实验设计，不替代研究人员对具体反应体系的专业判断。</p>");

    html += QStringLiteral("<h2>AI 可用资料</h2>");
'''
new_report_advice = '''    const auto experimentAdvice = ResearchAdvisor::experimentAdvice(records, result);
    html += QStringLiteral("<h2>下一步实验建议</h2>");
    if (experimentAdvice.isEmpty()) {
        html += QStringLiteral("<p>当前数据未触发额外实验建议。</p>");
    } else {
        html += QStringLiteral(
            "<table><tr><th>优先级</th><th>催化剂</th><th>建议</th><th>可执行性</th>"
            "<th>原因</th><th>下一步</th><th>依据</th></tr>");
        for (const auto& advice : experimentAdvice) {
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td><td>%7</td></tr>")
                .arg(escape(ResearchAdvisor::advicePriorityText(advice.priority)))
                .arg(escape(advice.catalyst))
                .arg(escape(advice.action))
                .arg(escape(QStringLiteral("%1 · %2/100").arg(advice.feasibility).arg(advice.feasibilityScore)))
                .arg(escape(advice.reason))
                .arg(escape(advice.target))
                .arg(escape(advice.basis));
        }
        html += QStringLiteral("</table>");
    }
    html += QStringLiteral(
        "<p class='note'><b>实验建议说明：</b>可执行性评分只评估当前数据完整度、实验条件一致性、建议跨度和公开实验窗口是否接近。"
        "它不代表设备安全许可，也不会替代反应器温压上限、气体安全、催化剂装填量和实验室 SOP 审核。</p>");

    const auto referenceEntries = ReferenceKnowledgeBase::entries();
    const auto referenceMatches = ReferenceKnowledgeBase::matchExperimentContext(records);
    html += QStringLiteral("<h2>内置公开参考库</h2>");
    html += QStringLiteral(
        "<p class='small'>参考库为离线事实快照，仅保存 PubChem 标识/基础物性和公开论文的 DOI、实验条件与时长摘要；不内置论文全文或图表。"
        "快照用于核对组分身份和判断建议时长是否处于公开研究量级，不会覆盖用户实验数据。</p>");
    html += QStringLiteral("<table><tr><th>来源</th><th>条目</th><th>标识符</th><th>关键信息</th><th>用途</th><th>快照</th></tr>");
    for (const auto& entry : referenceEntries) {
        html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td></tr>")
            .arg(escape(entry.source), escape(entry.title), escape(entry.identifier),
                 escape(entry.keyFacts), escape(entry.use), escape(entry.snapshotDate));
    }
    html += QStringLiteral("</table>");
    if (referenceMatches.isEmpty()) {
        html += QStringLiteral("<p>当前实验上下文没有匹配到足够接近的内置公开实验窗口，因此实验建议未强行套用文献时长。</p>");
    } else {
        html += QStringLiteral("<h3>与当前条件较接近的公开实验窗口</h3>");
        html += QStringLiteral("<table><tr><th>引用</th><th>DOI</th><th>公开时长</th><th>相关度</th><th>匹配说明</th></tr>");
        const qsizetype limit = qMin<qsizetype>(referenceMatches.size(), 3);
        for (qsizetype i = 0; i < limit; ++i) {
            const auto& match = referenceMatches[i];
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3 h</td><td>%4/100</td><td>%5</td></tr>")
                .arg(escape(match.reference.citation))
                .arg(escape(match.reference.doi))
                .arg(QString::number(match.reference.durationHours, 'g', 6))
                .arg(match.relevanceScore)
                .arg(escape(match.reason));
        }
        html += QStringLiteral("</table>");
    }

    html += QStringLiteral("<h2>AI 可用资料</h2>");
'''
replace_once("native/qt/src/reportexporter.cpp", old_report_advice, new_report_advice, "report advice and reference section")

# Native self-test: verify real reference snapshot and executability fields.
replace_once(
    "native/qt/src/main.cpp",
    '#include "reportexporter.h"\n#include "researchadvisor.h"\n',
    '#include "reportexporter.h"\n#include "referenceknowledge.h"\n#include "researchadvisor.h"\n',
    "main reference include",
)

old_builtin_test = '''        const auto builtInDatasets = catalyst::BuiltInDatasets::all();
        if (builtInDatasets.size() != 5) return 30;
        for (const auto& dataset : builtInDatasets) {
            if (dataset.name.trimmed().isEmpty() || dataset.records.isEmpty()) return 31;
        }

        const auto records = catalyst::CsvReader::demoData();
'''
new_builtin_test = '''        const auto builtInDatasets = catalyst::BuiltInDatasets::all();
        if (builtInDatasets.size() != 5) return 30;
        for (const auto& dataset : builtInDatasets) {
            if (dataset.name.trimmed().isEmpty() || dataset.records.isEmpty()) return 31;
        }
        const auto referenceEntries = catalyst::ReferenceKnowledgeBase::entries();
        if (referenceEntries.size() < 7) return 32;
        const auto referenceMatches = catalyst::ReferenceKnowledgeBase::matchExperimentContext(builtInDatasets.front().records);
        if (referenceMatches.isEmpty() || referenceMatches.front().relevanceScore < 45) return 33;

        const auto records = catalyst::CsvReader::demoData();
'''
replace_once("native/qt/src/main.cpp", old_builtin_test, new_builtin_test, "reference self-test setup")

old_advice_test = '''        const auto experimentAdvice = catalyst::ResearchAdvisor::experimentAdvice(records, result);
        if (experimentAdvice.isEmpty()) return 27;
        const auto comparisons = catalyst::ResearchAdvisor::pairComparisons(records, result);
'''
new_advice_test = '''        const auto experimentAdvice = catalyst::ResearchAdvisor::experimentAdvice(records, result);
        if (experimentAdvice.isEmpty()) return 27;
        bool hasExecutableAdvice = false;
        for (const auto& item : experimentAdvice) {
            if (item.feasibilityScore > 0 && !item.feasibility.isEmpty() && !item.basis.isEmpty()) {
                hasExecutableAdvice = true;
                break;
            }
        }
        if (!hasExecutableAdvice) return 34;
        const auto comparisons = catalyst::ResearchAdvisor::pairComparisons(records, result);
'''
replace_once("native/qt/src/main.cpp", old_advice_test, new_advice_test, "advice feasibility self-test")

print("Reference knowledge base and feasible advice integration applied.")
