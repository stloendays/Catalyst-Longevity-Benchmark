from pathlib import Path


def replace_once(path: str, old: str, new: str, label: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if old not in text:
        raise RuntimeError(f"missing {label} in {path}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


p = "native/qt/src/researchadvisor.cpp"
replace_once(
    p,
    '''double roundPracticalTime(double hours) {
    if (hours <= 0.0) return 0.0;
    double step = 1.0;
    if (hours >= 1000.0) step = 25.0;
    else if (hours >= 300.0) step = 10.0;
    else if (hours >= 120.0) step = 5.0;
    else if (hours >= 24.0) step = 2.0;
    return qMax(step, qRound(hours / step) * step);
}

double suggestedExtension(const QVector<Record>& inputRows) {
    if (inputRows.isEmpty()) return 0.0;
    const auto rows = sortedRows(inputRows);
    const double latest = rows.back().timeHours;
    const double typicalStep = medianPositiveStep(rows);
    double target = qMax(latest * 1.25, latest + 2.0 * typicalStep);
    if (latest > 0.0) target = qMin(target, latest * 2.0);
    return roundPracticalTime(target);
}

double largestGapMidpoint(const QVector<Record>& inputRows) {
    const auto rows = sortedRows(inputRows);
    if (rows.size() < 2) {
        const double base = rows.isEmpty() ? 0.0 : rows.front().timeHours;
        return roundPracticalTime(base + qMax(1.0, base * 0.25));
    }
    double bestGap = -1.0;
    double midpoint = 0.0;
    for (qsizetype i = 1; i < rows.size(); ++i) {
        const double gap = rows[i].timeHours - rows[i - 1].timeHours;
        if (gap > bestGap) {
            bestGap = gap;
            midpoint = (rows[i].timeHours + rows[i - 1].timeHours) / 2.0;
        }
    }
    return roundPracticalTime(midpoint);
}
''',
    '''double roundPracticalTime(double hours, double minimumGridHours = 0.0) {
    if (hours <= 0.0) return 0.0;
    double step = 1.0;
    if (hours >= 1000.0) step = 25.0;
    else if (hours >= 300.0) step = 10.0;
    else if (hours >= 120.0) step = 5.0;
    else if (hours >= 24.0) step = 2.0;
    if (minimumGridHours > 0.0) step = qMax(step, minimumGridHours);
    return qMax(step, qRound(hours / step) * step);
}

QString planningConstraintText(const ExperimentPlanningConstraints& constraints) {
    QStringList parts;
    if (constraints.maxAdditionalHoursPerStage > 0.0) {
        parts.append(QStringLiteral("单阶段最多追加 %1 h")
            .arg(QString::number(constraints.maxAdditionalHoursPerStage, 'g', 6)));
    }
    if (constraints.minSamplingIntervalHours > 0.0) {
        parts.append(QStringLiteral("最小采样间隔 %1 h")
            .arg(QString::number(constraints.minSamplingIntervalHours, 'g', 6)));
    }
    return parts.isEmpty()
        ? QStringLiteral("未设置额外排期约束")
        : QStringLiteral("已应用排期约束：%1").arg(parts.join(QStringLiteral("、")));
}

double suggestedExtension(
    const QVector<Record>& inputRows,
    const ExperimentPlanningConstraints& constraints) {
    if (inputRows.isEmpty()) return 0.0;
    const auto rows = sortedRows(inputRows);
    const double latest = rows.back().timeHours;
    const double typicalStep = medianPositiveStep(rows);
    double target = qMax(latest * 1.25, latest + 2.0 * typicalStep);
    if (latest > 0.0) target = qMin(target, latest * 2.0);
    if (constraints.maxAdditionalHoursPerStage > 0.0) {
        target = qMin(target, latest + constraints.maxAdditionalHoursPerStage);
    }
    target = roundPracticalTime(target, constraints.minSamplingIntervalHours);
    if (target <= latest) {
        const double fallback = constraints.minSamplingIntervalHours > 0.0
            ? constraints.minSamplingIntervalHours
            : qMax(1.0, typicalStep);
        target = latest + fallback;
    }
    return target;
}

double largestGapMidpoint(
    const QVector<Record>& inputRows,
    const ExperimentPlanningConstraints& constraints) {
    const auto rows = sortedRows(inputRows);
    if (rows.size() < 2) {
        const double base = rows.isEmpty() ? 0.0 : rows.front().timeHours;
        return roundPracticalTime(
            base + qMax(1.0, base * 0.25), constraints.minSamplingIntervalHours);
    }
    double bestGap = -1.0;
    double lower = 0.0;
    double upper = 0.0;
    for (qsizetype i = 1; i < rows.size(); ++i) {
        const double gap = rows[i].timeHours - rows[i - 1].timeHours;
        if (gap > bestGap) {
            bestGap = gap;
            lower = rows[i - 1].timeHours;
            upper = rows[i].timeHours;
        }
    }
    const double midpoint = (lower + upper) / 2.0;
    const double rounded = roundPracticalTime(midpoint, constraints.minSamplingIntervalHours);
    if (rounded > lower + 1.0e-9 && rounded < upper - 1.0e-9) return rounded;
    return midpoint;
}
''',
    "planning time helpers",
)

replace_once(
    p,
    '''double referenceAdjustedTarget(
    double latest,
    double dataTarget,
    const QVector<ReferenceMatch>& matches,
    QString* matchedCitation) {''',
    '''double referenceAdjustedTarget(
    double latest,
    double dataTarget,
    const QVector<ReferenceMatch>& matches,
    const ExperimentPlanningConstraints& constraints,
    QString* matchedCitation) {''',
    "reference target signature",
)
replace_once(
    p,
    '''    return roundPracticalTime(bestTarget);
}

bool pairHasMismatch''',
    '''    if (constraints.maxAdditionalHoursPerStage > 0.0) {
        bestTarget = qMin(bestTarget, latest + constraints.maxAdditionalHoursPerStage);
    }
    return roundPracticalTime(bestTarget, constraints.minSamplingIntervalHours);
}

bool pairHasMismatch''',
    "reference target constraints",
)

replace_once(
    p,
    '''QVector<ExperimentAdvice> ResearchAdvisor::experimentAdvice(
    const QVector<Record>& records,
    const AnalysisResult& analysis) {''',
    '''QVector<ExperimentAdvice> ResearchAdvisor::experimentAdvice(
    const QVector<Record>& records,
    const AnalysisResult& analysis,
    const ExperimentPlanningConstraints& constraints) {''',
    "advice signature",
)
replace_once(p, "const double midpoint = largestGapMidpoint(rows);", "const double midpoint = largestGapMidpoint(rows, constraints);", "largest gap call")
replace_once(p, "const double dataTarget = suggestedExtension(rows);", "const double dataTarget = suggestedExtension(rows, constraints);", "extension call")
replace_once(
    p,
    "const double target = referenceAdjustedTarget(latest, dataTarget, referenceMatches, &matchedCitation);\n            const double middle = roundPracticalTime(latest + (target - latest) / 2.0);",
    "const double target = referenceAdjustedTarget(latest, dataTarget, referenceMatches, constraints, &matchedCitation);\n            const double middle = roundPracticalTime(\n                latest + (target - latest) / 2.0, constraints.minSamplingIntervalHours);",
    "reference adjusted call",
)
replace_once(
    p,
    '''                hasReferenceSupport
                    ? QStringLiteral("当前采样间隔 + 公开同类实验参照：%1").arg(referenceBasis)
                    : QStringLiteral("当前采样间隔与寿命下限规则；%1").arg(referenceBasis)));''',
    '''                hasReferenceSupport
                    ? QStringLiteral("当前采样间隔 + 公开同类实验参照：%1；%2")
                        .arg(referenceBasis, planningConstraintText(constraints))
                    : QStringLiteral("当前采样间隔与寿命下限规则；%1；%2")
                        .arg(referenceBasis, planningConstraintText(constraints))));''',
    "right censored basis",
)

old_interval = '''            const double lower = *summary.t90.lowerHours;
            const double upper = *summary.t90.upperHours;
            const double midpoint = roundPracticalTime((lower + upper) / 2.0);
            const int score = baseFeasibilityScore(rows, analysis, hasReferenceSupport);
            advice.append(makeAdvice(
                AdvicePriority::Important,
                summary.catalyst,
                QStringLiteral("只补一个 T90 关键点"),
                QStringLiteral("T90 目前只知道落在 %1–%2 h；与其整套重做，先在区间中部补点能直接缩小不确定范围。")
                    .arg(QString::number(lower, 'g', 6), QString::number(upper, 'g', 6)),
                QStringLiteral("%1，在约 %2 h 增加 1 个采样点；补测后重新计算 T90 区间，再决定是否需要第二个点。")
                    .arg(keepConditions, QString::number(midpoint, 'g', 6)),
                score,
                QStringLiteral("依据当前 T90 实测区间的二分加密原则；公开参照只用于校验测试量级，不替代本组数据。")));'''
new_interval = '''            const double lower = *summary.t90.lowerHours;
            const double upper = *summary.t90.upperHours;
            const double width = upper - lower;
            const bool spacingBlocksRefinement = constraints.minSamplingIntervalHours > 0.0
                && constraints.minSamplingIntervalHours >= width - 1.0e-9;
            const double midpoint = spacingBlocksRefinement
                ? (lower + upper) / 2.0
                : roundPracticalTime((lower + upper) / 2.0, constraints.minSamplingIntervalHours);
            int score = baseFeasibilityScore(rows, analysis, hasReferenceSupport);
            if (spacingBlocksRefinement) score = qMin(score, 62);
            advice.append(makeAdvice(
                AdvicePriority::Important,
                summary.catalyst,
                spacingBlocksRefinement
                    ? QStringLiteral("先确认采样能力")
                    : QStringLiteral("只补一个 T90 关键点"),
                spacingBlocksRefinement
                    ? QStringLiteral("T90 位于 %1–%2 h，但设置的最小采样间隔 %3 h 已不小于该区间宽度，当前排期约束下无法继续有效二分。")
                        .arg(QString::number(lower, 'g', 6), QString::number(upper, 'g', 6),
                             QString::number(constraints.minSamplingIntervalHours, 'g', 6))
                    : QStringLiteral("T90 目前只知道落在 %1–%2 h；与其整套重做，先在区间中部补点能直接缩小不确定范围。")
                        .arg(QString::number(lower, 'g', 6), QString::number(upper, 'g', 6)),
                spacingBlocksRefinement
                    ? QStringLiteral("保持原 T90 区间；若该区间必须继续缩小，先确认仪器和排期是否允许小于 %1 h 的采样间隔。")
                        .arg(QString::number(width, 'g', 6))
                    : QStringLiteral("%1，在约 %2 h 增加 1 个采样点；补测后重新计算 T90 区间，再决定是否需要第二个点。")
                        .arg(keepConditions, QString::number(midpoint, 'g', 6)),
                score,
                QStringLiteral("依据当前 T90 实测区间的二分加密原则；公开参照只用于校验测试量级；%1。")
                    .arg(planningConstraintText(constraints))));'''
replace_once(p, old_interval, new_interval, "interval constraint handling")

replace_once(
    p,
    '''            const double upper = summary.t90.upperHours.value_or(summary.initialTimeHours);
            const double target = upper > 0.0 ? roundPracticalTime(upper * 0.5) : 0.0;''',
    '''            const double upper = summary.t90.upperHours.value_or(summary.initialTimeHours);
            double target = upper > 0.0
                ? roundPracticalTime(upper * 0.5, constraints.minSamplingIntervalHours)
                : 0.0;
            if (upper > 0.0 && target >= upper) target = upper * 0.5;''',
    "left censored constrained target",
)
replace_once(
    p,
    '''                QStringLiteral("依据当前左侧区间；建议时间取现有上界的一半并按可操作时间刻度取整。")));''',
    '''                QStringLiteral("依据当前左侧区间；建议时间取现有上界的一半并按可操作时间刻度取整；%1。")
                    .arg(planningConstraintText(constraints))));''',
    "left censored basis",
)

# MainWindow UI controls and constrained advice use.
p = "native/qt/src/mainwindow.cpp"
replace_once(p, '#include <QFileInfo>\n#include <QFont>\n', '#include <QFileInfo>\n#include <QFont>\n#include <QDoubleSpinBox>\n', "double spin include")
old_intro = '''    adviceLayout->addWidget(muted(QStringLiteral(
        "建议会同时检查当前采样间隔、实验条件完整度、建议跨度和可匹配的公开实验窗口。公开参考只用于校验量级，不会直接替代你的实验条件。")));
    adviceTable_ = new QTableWidget(0, 7);'''
new_intro = '''    adviceLayout->addWidget(muted(QStringLiteral(
        "建议会同时检查当前采样间隔、实验条件完整度、建议跨度和可匹配的公开实验窗口。公开参考只用于校验量级，不会直接替代你的实验条件。")));

    auto* constraintFrame = new QFrame;
    constraintFrame->setObjectName(QStringLiteral("infoPanel"));
    auto* constraintLayout = new QHBoxLayout(constraintFrame);
    constraintLayout->setContentsMargins(14, 10, 14, 10);
    constraintLayout->setSpacing(10);
    auto* constraintTitle = new QLabel(QStringLiteral("实验排期约束（可选）"));
    constraintTitle->setObjectName(QStringLiteral("sectionTitle"));
    constraintLayout->addWidget(constraintTitle);
    constraintLayout->addWidget(new QLabel(QStringLiteral("单阶段最多追加")));
    maxAdditionalHoursSpin_ = new QDoubleSpinBox;
    maxAdditionalHoursSpin_->setRange(0.0, 100000.0);
    maxAdditionalHoursSpin_->setDecimals(1);
    maxAdditionalHoursSpin_->setSingleStep(12.0);
    maxAdditionalHoursSpin_->setSuffix(QStringLiteral(" h"));
    maxAdditionalHoursSpin_->setSpecialValueText(QStringLiteral("自动"));
    maxAdditionalHoursSpin_->setToolTip(QStringLiteral("限制每个建议阶段相对当前最长测试时间最多再增加多少小时；0 表示自动。它不是反应器安全上限。"));
    constraintLayout->addWidget(maxAdditionalHoursSpin_);
    constraintLayout->addWidget(new QLabel(QStringLiteral("最小采样间隔")));
    minSamplingIntervalSpin_ = new QDoubleSpinBox;
    minSamplingIntervalSpin_->setRange(0.0, 10000.0);
    minSamplingIntervalSpin_->setDecimals(1);
    minSamplingIntervalSpin_->setSingleStep(1.0);
    minSamplingIntervalSpin_->setSuffix(QStringLiteral(" h"));
    minSamplingIntervalSpin_->setSpecialValueText(QStringLiteral("自动"));
    minSamplingIntervalSpin_->setToolTip(QStringLiteral("按设备、人员排期或分析频率设置能够执行的最小采样间隔；0 表示自动。"));
    constraintLayout->addWidget(minSamplingIntervalSpin_);
    constraintLayout->addStretch();
    constraintLayout->addWidget(muted(QStringLiteral("0 = 自动；仅用于排期可执行性，不代表设备安全许可")));
    adviceLayout->addWidget(constraintFrame);
    connect(maxAdditionalHoursSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        refreshResearchSupportViews();
    });
    connect(minSamplingIntervalSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        refreshResearchSupportViews();
    });

    adviceTable_ = new QTableWidget(0, 7);'''
replace_once(p, old_intro, new_intro, "planning constraint UI")
replace_once(
    p,
    '''    if (adviceTable_) {
        const auto advice = ResearchAdvisor::experimentAdvice(records_, analysis_);''',
    '''    if (adviceTable_) {
        ExperimentPlanningConstraints constraints;
        if (maxAdditionalHoursSpin_) constraints.maxAdditionalHoursPerStage = maxAdditionalHoursSpin_->value();
        if (minSamplingIntervalSpin_) constraints.minSamplingIntervalHours = minSamplingIntervalSpin_->value();
        const auto advice = ResearchAdvisor::experimentAdvice(records_, analysis_, constraints);''',
    "constrained advice refresh",
)

# MainWindow header: UI control pointers.
p = "native/qt/src/mainwindow.h"
replace_once(p, 'class QCloseEvent;\nclass QLabel;\n', 'class QCloseEvent;\nclass QDoubleSpinBox;\nclass QLabel;\n', "spin forward declaration")
replace_once(
    p,
    '''    QTableWidget* adviceTable_ = nullptr;
    QLabel* referenceMatchSummary_ = nullptr;''',
    '''    QTableWidget* adviceTable_ = nullptr;
    QDoubleSpinBox* maxAdditionalHoursSpin_ = nullptr;
    QDoubleSpinBox* minSamplingIntervalSpin_ = nullptr;
    QLabel* referenceMatchSummary_ = nullptr;''',
    "spin members",
)

# Report exporter receives the same user planning constraints.
p = "native/qt/src/reportexporter.h"
replace_once(p, '#include "models.h"\n', '#include "models.h"\n#include "researchadvisor.h"\n', "report constraints include")
replace_once(
    p,
    '''        const QString& sourceLabel,
        const QVector<EvidenceItem>& evidenceItems,
        QString* errorMessage = nullptr);
};''',
    '''        const QString& sourceLabel,
        const QVector<EvidenceItem>& evidenceItems,
        QString* errorMessage = nullptr,
        const ExperimentPlanningConstraints& planningConstraints = ExperimentPlanningConstraints{});
};''',
    "report constrained overload",
)

p = "native/qt/src/reportexporter.cpp"
replace_once(
    p,
    '''    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems) {''',
    '''    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems,
    const ExperimentPlanningConstraints& planningConstraints) {''',
    "build html constraint parameter",
)
replace_once(
    p,
    'const auto experimentAdvice = ResearchAdvisor::experimentAdvice(records, result);',
    'const auto experimentAdvice = ResearchAdvisor::experimentAdvice(records, result, planningConstraints);',
    "report constrained advice call",
)
replace_once(
    p,
    '''    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage) {''',
    '''    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage,
    const ExperimentPlanningConstraints& planningConstraints) {''',
    "write pdf constraint parameter",
)
replace_once(
    p,
    'document.setHtml(buildHtml(records, result, sourceLabel, evidenceItems));',
    'document.setHtml(buildHtml(records, result, sourceLabel, evidenceItems, planningConstraints));',
    "build html constrained call",
)
replace_once(p, 'return writePdf(path, {}, result, sourceLabel, {}, errorMessage);', 'return writePdf(path, {}, result, sourceLabel, {}, errorMessage, {});', "simple report call")
replace_once(p, 'return writePdf(path, {}, result, sourceLabel, evidenceItems, errorMessage);', 'return writePdf(path, {}, result, sourceLabel, evidenceItems, errorMessage, {});', "evidence report call")
replace_once(
    p,
    '''    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage) {
    return writePdf(path, records, result, sourceLabel, evidenceItems, errorMessage);
}''',
    '''    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage,
    const ExperimentPlanningConstraints& planningConstraints) {
    return writePdf(path, records, result, sourceLabel, evidenceItems, errorMessage, planningConstraints);
}''',
    "records report definition",
)

# MainWindow export uses current planning controls.
p = "native/qt/src/mainwindow.cpp"
replace_once(
    p,
    '''    if (!ReportExporter::exportPdf(path, records_, analysis_, sourceLabelText_, evidence, &message)) {''',
    '''    ExperimentPlanningConstraints planningConstraints;
    if (maxAdditionalHoursSpin_) planningConstraints.maxAdditionalHoursPerStage = maxAdditionalHoursSpin_->value();
    if (minSamplingIntervalSpin_) planningConstraints.minSamplingIntervalHours = minSamplingIntervalSpin_->value();
    if (!ReportExporter::exportPdf(
            path, records_, analysis_, sourceLabelText_, evidence, &message, planningConstraints)) {''',
    "main window constrained report",
)

# Add report note showing planning constraints when present.
p = "native/qt/src/reportexporter.cpp"
marker = '''    const auto experimentAdvice = ResearchAdvisor::experimentAdvice(records, result, planningConstraints);
    html += QStringLiteral("<h2>下一步实验建议</h2>");'''
replacement = '''    const auto experimentAdvice = ResearchAdvisor::experimentAdvice(records, result, planningConstraints);
    html += QStringLiteral("<h2>下一步实验建议</h2>");
    if (planningConstraints.maxAdditionalHoursPerStage > 0.0 || planningConstraints.minSamplingIntervalHours > 0.0) {
        QStringList constraints;
        if (planningConstraints.maxAdditionalHoursPerStage > 0.0) {
            constraints.append(QStringLiteral("单阶段最多追加 %1 h")
                .arg(QString::number(planningConstraints.maxAdditionalHoursPerStage, 'g', 6)));
        }
        if (planningConstraints.minSamplingIntervalHours > 0.0) {
            constraints.append(QStringLiteral("最小采样间隔 %1 h")
                .arg(QString::number(planningConstraints.minSamplingIntervalHours, 'g', 6)));
        }
        html += QStringLiteral("<p class='small'>本次实验排期约束：%1。该约束仅用于计划可执行性，不代表设备安全限值。</p>")
            .arg(escape(constraints.join(QStringLiteral("、"))));
    }'''
replace_once(p, marker, replacement, "report planning constraint note")

print("Experiment planning constraints integrated.")
