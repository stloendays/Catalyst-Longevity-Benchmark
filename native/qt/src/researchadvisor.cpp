#include "researchadvisor.h"

#include "referenceknowledge.h"

#include <QMap>
#include <QSet>
#include <QtMath>
#include <algorithm>
#include <limits>

namespace catalyst {

namespace {

QString normalizedNumber(double value) {
    return QString::number(value, 'g', 15);
}

bool sameTime(double a, double b) {
    return qAbs(a - b) < 1.0e-9;
}

QStringList missingConditionFields(const QVector<Record>& rows) {
    bool missingTemperature = false;
    bool missingSpaceVelocity = false;
    bool missingPressure = false;
    bool missingFeed = false;
    for (const auto& row : rows) {
        missingTemperature = missingTemperature || !row.temperatureC.has_value();
        missingSpaceVelocity = missingSpaceVelocity || (!row.gHSV.has_value() && !row.wHSV.has_value());
        missingPressure = missingPressure || !row.pressure.has_value();
        missingFeed = missingFeed || row.feedRatio.trimmed().isEmpty();
    }

    QStringList fields;
    if (missingTemperature) fields.append(QStringLiteral("温度"));
    if (missingSpaceVelocity) fields.append(QStringLiteral("空速"));
    if (missingPressure) fields.append(QStringLiteral("压力"));
    if (missingFeed) fields.append(QStringLiteral("进料条件"));
    return fields;
}

QStringList internallyVariableFields(const QVector<Record>& rows) {
    QSet<QString> temperatures;
    QSet<QString> spaceVelocities;
    QSet<QString> pressures;
    QSet<QString> feeds;

    for (const auto& row : rows) {
        if (row.temperatureC) temperatures.insert(normalizedNumber(*row.temperatureC));
        if (row.gHSV) spaceVelocities.insert(QStringLiteral("GHSV:") + normalizedNumber(*row.gHSV));
        if (row.wHSV) spaceVelocities.insert(QStringLiteral("WHSV:") + normalizedNumber(*row.wHSV));
        if (row.pressure) pressures.insert(normalizedNumber(*row.pressure));
        if (!row.feedRatio.trimmed().isEmpty()) feeds.insert(row.feedRatio.trimmed().toCaseFolded());
    }

    QStringList fields;
    if (temperatures.size() > 1) fields.append(QStringLiteral("温度"));
    if (spaceVelocities.size() > 1) fields.append(QStringLiteral("空速"));
    if (pressures.size() > 1) fields.append(QStringLiteral("压力"));
    if (feeds.size() > 1) fields.append(QStringLiteral("进料条件"));
    return fields;
}

QVector<Record> sortedRows(QVector<Record> rows) {
    std::sort(rows.begin(), rows.end(), [](const Record& a, const Record& b) {
        return a.timeHours < b.timeHours;
    });
    return rows;
}

double medianPositiveStep(const QVector<Record>& inputRows) {
    const auto rows = sortedRows(inputRows);
    QVector<double> deltas;
    for (qsizetype i = 1; i < rows.size(); ++i) {
        const double delta = rows[i].timeHours - rows[i - 1].timeHours;
        if (delta > 1.0e-9) deltas.append(delta);
    }
    if (deltas.isEmpty()) return rows.isEmpty() ? 1.0 : qMax(1.0, rows.back().timeHours * 0.25);
    std::sort(deltas.begin(), deltas.end());
    return deltas[deltas.size() / 2];
}

double roundPracticalTime(double hours, double minimumGridHours = 0.0) {
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
    const double userCap = constraints.maxAdditionalHoursPerStage > 0.0
        ? latest + constraints.maxAdditionalHoursPerStage
        : std::numeric_limits<double>::max();
    target = qMin(target, userCap);
    target = roundPracticalTime(target, constraints.minSamplingIntervalHours);
    if (target > userCap) target = userCap;
    if (target <= latest) {
        const double fallback = constraints.minSamplingIntervalHours > 0.0
            ? constraints.minSamplingIntervalHours
            : qMax(1.0, typicalStep);
        const double allowedFallback = constraints.maxAdditionalHoursPerStage > 0.0
            ? qMin(fallback, constraints.maxAdditionalHoursPerStage)
            : fallback;
        target = latest + qMax(0.1, allowedFallback);
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

QString constantConditionText(const QVector<Record>& rows) {
    QSet<QString> temperatures;
    QSet<QString> ghsvValues;
    QSet<QString> whsvValues;
    QSet<QString> pressures;
    QSet<QString> feeds;
    for (const auto& row : rows) {
        if (row.temperatureC) temperatures.insert(normalizedNumber(*row.temperatureC));
        if (row.gHSV) ghsvValues.insert(normalizedNumber(*row.gHSV));
        if (row.wHSV) whsvValues.insert(normalizedNumber(*row.wHSV));
        if (row.pressure) pressures.insert(normalizedNumber(*row.pressure));
        if (!row.feedRatio.trimmed().isEmpty()) feeds.insert(row.feedRatio.trimmed());
    }

    QStringList parts;
    if (temperatures.size() == 1) parts.append(QStringLiteral("温度 %1 ℃").arg(*temperatures.cbegin()));
    if (ghsvValues.size() == 1) parts.append(QStringLiteral("GHSV %1").arg(*ghsvValues.cbegin()));
    else if (whsvValues.size() == 1) parts.append(QStringLiteral("WHSV %1").arg(*whsvValues.cbegin()));
    if (pressures.size() == 1) parts.append(QStringLiteral("压力 %1 bar").arg(*pressures.cbegin()));
    if (feeds.size() == 1) parts.append(QStringLiteral("进料 %1").arg(*feeds.cbegin()));
    return parts.join(QStringLiteral("、"));
}

int baseFeasibilityScore(
    const QVector<Record>& rows,
    const AnalysisResult& analysis,
    bool hasReferenceSupport,
    double extensionRatio = 1.0) {
    int score = 92;
    score -= qMin(20, static_cast<int>(missingConditionFields(rows).size()) * 5);
    score -= qMin(24, static_cast<int>(internallyVariableFields(rows).size()) * 12);
    if (rows.size() < 3) score -= 15;
    if (analysis.conditionAudit.blocksDirectRanking()) score -= 12;
    if (extensionRatio > 1.75) score -= 8;
    if (hasReferenceSupport) score += 4;
    return qBound(35, score, 98);
}

QString feasibilityLabel(int score) {
    if (score >= 85) return QStringLiteral("较高");
    if (score >= 70) return QStringLiteral("中等");
    return QStringLiteral("需确认");
}

ExperimentAdvice makeAdvice(
    AdvicePriority priority,
    const QString& catalyst,
    const QString& action,
    const QString& reason,
    const QString& target,
    int score,
    const QString& basis) {
    ExperimentAdvice item;
    item.priority = priority;
    item.catalyst = catalyst;
    item.action = action;
    item.reason = reason;
    item.target = target;
    item.feasibilityScore = qBound(0, score, 100);
    item.feasibility = feasibilityLabel(item.feasibilityScore);
    item.basis = basis;
    return item;
}

double referenceAdjustedTarget(
    double latest,
    double dataTarget,
    const QVector<ReferenceMatch>& matches,
    const ExperimentPlanningConstraints& constraints,
    QString* matchedCitation) {
    double bestTarget = dataTarget;
    double bestRelativeDistance = std::numeric_limits<double>::max();
    for (const auto& match : matches) {
        const double duration = match.reference.durationHours;
        if (duration <= latest || duration <= 0.0 || dataTarget <= 0.0) continue;
        if (latest > 0.0 && duration > latest * 2.5) continue;
        const double relativeDistance = qAbs(duration - dataTarget) / dataTarget;
        if (relativeDistance <= 0.35 && relativeDistance < bestRelativeDistance) {
            bestRelativeDistance = relativeDistance;
            bestTarget = duration;
            if (matchedCitation) {
                *matchedCitation = QStringLiteral("%1（%2）")
                    .arg(match.reference.citation, match.reference.doi);
            }
        }
    }
    const double userCap = constraints.maxAdditionalHoursPerStage > 0.0
        ? latest + constraints.maxAdditionalHoursPerStage
        : std::numeric_limits<double>::max();
    bestTarget = qMin(bestTarget, userCap);
    bestTarget = roundPracticalTime(bestTarget, constraints.minSamplingIntervalHours);
    if (bestTarget > userCap) bestTarget = userCap;
    if (bestTarget <= latest && constraints.maxAdditionalHoursPerStage > 0.0) {
        bestTarget = latest + qMax(0.1, constraints.maxAdditionalHoursPerStage);
    }
    return bestTarget;
}

bool pairHasMismatch(const ConditionAudit& audit, const QString& a, const QString& b) {
    for (const auto& mismatch : audit.pairMismatches) {
        if ((mismatch.catalystA == a && mismatch.catalystB == b)
            || (mismatch.catalystA == b && mismatch.catalystB == a)) {
            return true;
        }
    }
    return false;
}

std::optional<double> performanceAt(const QVector<Record>& rows, double timeHours) {
    for (const auto& row : rows) {
        if (sameTime(row.timeHours, timeHours)) return row.performance;
    }
    return std::nullopt;
}

} // namespace

QString DataCheckResult::statusText() const {
    if (items.isEmpty()) return QStringLiteral("暂无结果");
    if (errorCount > 0) return QStringLiteral("需要处理");
    if (warningCount > 0) return QStringLiteral("基本可用");
    return QStringLiteral("数据良好");
}

QString ResearchAdvisor::checkLevelText(CheckLevel level) {
    switch (level) {
    case CheckLevel::Info:
        return QStringLiteral("提示");
    case CheckLevel::Warning:
        return QStringLiteral("建议处理");
    case CheckLevel::Error:
        return QStringLiteral("需要处理");
    }
    return QStringLiteral("提示");
}

QString ResearchAdvisor::advicePriorityText(AdvicePriority priority) {
    switch (priority) {
    case AdvicePriority::Normal:
        return QStringLiteral("一般");
    case AdvicePriority::Important:
        return QStringLiteral("建议");
    case AdvicePriority::High:
        return QStringLiteral("优先");
    }
    return QStringLiteral("一般");
}

DataCheckResult ResearchAdvisor::checkData(const QVector<Record>& records, const AnalysisResult& analysis) {
    DataCheckResult result;
    if (records.isEmpty()) {
        result.score = 0;
        result.warningCount = 1;
        result.items.append({
            CheckLevel::Warning,
            QStringLiteral("数据"),
            QStringLiteral("还没有导入实验数据"),
            QStringLiteral("导入 CSV 或 Excel 后即可自动检查数据完整性。")});
        return result;
    }

    QMap<QString, QVector<Record>> grouped;
    QSet<QString> observationKeys;
    for (const auto& record : records) {
        const QString catalystName = record.catalyst.trimmed();
        grouped[catalystName].append(record);

        if (record.timeHours < 0.0) {
            result.items.append({CheckLevel::Error, catalystName,
                QStringLiteral("发现负的测试时间"),
                QStringLiteral("请检查时间单位或原始记录。")});
        }
        if (record.performance <= 0.0) {
            result.items.append({CheckLevel::Error, catalystName,
                QStringLiteral("性能值小于或等于 0"),
                QStringLiteral("请确认性能指标和单位；初始性能为 0 时无法计算保持率。")});
        }

        const QString key = catalystName + QLatin1Char('|') + normalizedNumber(record.timeHours);
        if (observationKeys.contains(key)) {
            result.items.append({CheckLevel::Warning, catalystName,
                QStringLiteral("同一时间点存在重复记录"),
                QStringLiteral("建议核对重复行，避免同一观测被重复计入。")});
        }
        observationKeys.insert(key);
    }

    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
        const QString& catalystName = it.key();
        const auto& rows = it.value();
        if (catalystName.isEmpty()) {
            result.items.append({CheckLevel::Error, QStringLiteral("数据"),
                QStringLiteral("存在未填写催化剂名称的记录"),
                QStringLiteral("请补充催化剂名称后再进行分组分析。")});
            continue;
        }

        if (rows.size() < 3) {
            result.items.append({CheckLevel::Warning, catalystName,
                QStringLiteral("时间点较少"),
                QStringLiteral("建议至少保留 3 个时间点，以便判断性能变化趋势。")});
        }

        const QStringList missing = missingConditionFields(rows);
        if (!missing.isEmpty()) {
            result.items.append({CheckLevel::Info, catalystName,
                QStringLiteral("实验条件未填写完整：%1").arg(missing.join(QStringLiteral("、"))),
                QStringLiteral("补全条件后，跨催化剂比较会更可靠。")});
        }

        const QStringList variable = internallyVariableFields(rows);
        if (!variable.isEmpty()) {
            result.items.append({CheckLevel::Warning, catalystName,
                QStringLiteral("同一催化剂的条件发生变化：%1").arg(variable.join(QStringLiteral("、"))),
                QStringLiteral("若属于不同实验批次，建议拆分为不同数据组后再比较。")});
        }

        bool missingMetric = false;
        for (const auto& row : rows) {
            if (row.metric.trimmed().isEmpty()) {
                missingMetric = true;
                break;
            }
        }
        if (missingMetric) {
            result.items.append({CheckLevel::Info, catalystName,
                QStringLiteral("性能指标名称未填写完整"),
                QStringLiteral("建议注明转化率、选择性或其他指标名称，便于报告和资料核对。")});
        }
    }

    if (analysis.conditionAudit.blocksDirectRanking()) {
        result.items.append({CheckLevel::Error, QStringLiteral("催化剂比较"),
            QStringLiteral("不同催化剂的实验条件不一致"),
            QStringLiteral("请统一实验条件，或仅查看各自趋势，不进行直接排名。")});
    } else if (analysis.conditionAudit.status == ConditionAuditStatus::ConditionsNotProvided) {
        result.items.append({CheckLevel::Warning, QStringLiteral("催化剂比较"),
            QStringLiteral("实验条件信息不足"),
            QStringLiteral("可以先查看趋势，但正式比较前建议补全温度、空速、压力和进料条件。")});
    }

    for (const auto& item : result.items) {
        if (item.level == CheckLevel::Error) ++result.errorCount;
        else if (item.level == CheckLevel::Warning) ++result.warningCount;
        else ++result.infoCount;
    }
    result.score = qBound(0, 100 - result.errorCount * 20 - result.warningCount * 8 - result.infoCount * 2, 100);
    return result;
}

QVector<ExperimentAdvice> ResearchAdvisor::experimentAdvice(
    const QVector<Record>& records,
    const AnalysisResult& analysis,
    const ExperimentPlanningConstraints& constraints) {
    QVector<ExperimentAdvice> advice;
    if (records.isEmpty()) {
        advice.append(makeAdvice(
            AdvicePriority::High,
            QStringLiteral("全部"),
            QStringLiteral("先导入实验数据"),
            QStringLiteral("当前没有可用于寿命分析的记录。"),
            QStringLiteral("建议每个催化剂至少准备 3 个时间点，并记录温度、空速、压力和进料条件。"),
            45,
            QStringLiteral("基于最低数据完整性要求；当前没有足够信息评估设备与反应体系约束。")));
        return advice;
    }

    QMap<QString, QVector<Record>> grouped;
    for (const auto& record : records) grouped[record.catalyst.trimmed()].append(record);
    const auto referenceMatches = ReferenceKnowledgeBase::matchExperimentContext(records);
    const bool hasReferenceSupport = !referenceMatches.isEmpty();
    const QString referenceBasis = ReferenceKnowledgeBase::compactMatchText(referenceMatches, 2);

    if (analysis.conditionAudit.blocksDirectRanking()) {
        advice.append(makeAdvice(
            AdvicePriority::High,
            QStringLiteral("全部"),
            QStringLiteral("先统一比较条件"),
            QStringLiteral("当前不同催化剂之间存在明确实验条件差异，继续加时测试不能解决可比性问题。"),
            QStringLiteral("先确定一组统一的温度、空速、压力和进料条件，再对需要横向比较的样品各补一组同条件数据。"),
            90,
            QStringLiteral("依据当前条件检查结果；此建议优先于任何寿命排名或文献时长参照。")));
    }

    for (const auto& summary : analysis.catalysts) {
        const auto rows = grouped.value(summary.catalyst);
        const QString conditionText = constantConditionText(rows);
        const QString keepConditions = conditionText.isEmpty()
            ? QStringLiteral("尽量保持本轮实验条件不变")
            : QStringLiteral("保持 %1 不变").arg(conditionText);

        if (summary.observations < 3) {
            const double midpoint = largestGapMidpoint(rows, constraints);
            const QString target = midpoint > 0.0
                ? QStringLiteral("先在约 %1 h 补 1 个时间点，使该样品至少达到 3 个观测点；%2。")
                    .arg(QString::number(midpoint, 'g', 6), keepConditions)
                : QStringLiteral("先补足至少 3 个时间点，并保持实验条件一致。");
            const int score = baseFeasibilityScore(rows, analysis, false);
            advice.append(makeAdvice(
                AdvicePriority::High,
                summary.catalyst,
                QStringLiteral("先补关键时间点"),
                QStringLiteral("当前只有 %1 个观测点，直接延长总时长前先补中间点更容易判断趋势。")
                    .arg(summary.observations),
                target,
                score,
                QStringLiteral("依据当前采样间隔的最大空档；不依赖外部文献，不新增未经测量的数据。")));
        }

        if (summary.t90.status == ThresholdStatus::RightCensored) {
            const auto sorted = sortedRows(rows);
            const double latest = sorted.isEmpty() ? summary.latestTimeHours : sorted.back().timeHours;
            const double dataTarget = suggestedExtension(rows, constraints);
            QString matchedCitation;
            const double target = referenceAdjustedTarget(latest, dataTarget, referenceMatches, constraints, &matchedCitation);
            const double middle = roundPracticalTime(
                latest + (target - latest) / 2.0, constraints.minSamplingIntervalHours);
            const double ratio = latest > 0.0 ? target / latest : 1.0;
            int score = baseFeasibilityScore(rows, analysis, hasReferenceSupport, ratio);
            QString targetText = QStringLiteral("%1；下一阶段先做到约 %2 h")
                .arg(keepConditions, QString::number(target, 'g', 6));
            if (middle > latest + 1.0e-9 && middle < target - 1.0e-9) {
                targetText += QStringLiteral("，并在约 %1 h 保留一个中间采样点").arg(QString::number(middle, 'g', 6));
            }
            targetText += QStringLiteral("。到达该阶段后先看是否仍高于 T90，再决定是否继续，不一次性无限外推。");
            if (!matchedCitation.isEmpty()) {
                targetText += QStringLiteral(" 该时长与公开同类实验窗口接近：%1。").arg(matchedCitation);
            }
            advice.append(makeAdvice(
                AdvicePriority::Important,
                summary.catalyst,
                QStringLiteral("分阶段延长寿命测试"),
                QStringLiteral("测试结束时仍未达到 T90，目前只能得到寿命下限。分阶段延长比直接指定一个很长终点更可执行。"),
                targetText,
                score,
                hasReferenceSupport
                    ? QStringLiteral("当前采样间隔 + 公开同类实验参照：%1；%2")
                        .arg(referenceBasis, planningConstraintText(constraints))
                    : QStringLiteral("当前采样间隔与寿命下限规则；%1；%2")
                        .arg(referenceBasis, planningConstraintText(constraints))));
        } else if (summary.t90.status == ThresholdStatus::Interval
                   && summary.t90.lowerHours && summary.t90.upperHours) {
            const double lower = *summary.t90.lowerHours;
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
                    .arg(planningConstraintText(constraints))));
        } else if (summary.t90.status == ThresholdStatus::LeftCensored) {
            const double upper = summary.t90.upperHours.value_or(summary.initialTimeHours);
            double target = upper > 0.0
                ? roundPracticalTime(upper * 0.5, constraints.minSamplingIntervalHours)
                : 0.0;
            if (upper > 0.0 && target >= upper) target = upper * 0.5;
            const int score = baseFeasibilityScore(rows, analysis, false);
            advice.append(makeAdvice(
                AdvicePriority::High,
                summary.catalyst,
                QStringLiteral("把首个采样提前"),
                QStringLiteral("第一次观测时已经达到 T90，现有采样从一开始就错过了阈值。"),
                target > 0.0
                    ? QStringLiteral("保留 0 h 基线，%1，并在约 %2 h 增加一个早期采样点。")
                        .arg(keepConditions, QString::number(target, 'g', 6))
                    : QStringLiteral("保留 0 h 基线，并增加更早的首个采样点。"),
                score,
                QStringLiteral("依据当前左侧区间；建议时间取现有上界的一半并按可操作时间刻度取整；%1。")
                    .arg(planningConstraintText(constraints))));
        }

        const QStringList missing = missingConditionFields(rows);
        if (!missing.isEmpty()) {
            const int score = qBound(55, 92 - static_cast<int>(missing.size()) * 5, 90);
            advice.append(makeAdvice(
                AdvicePriority::Normal,
                summary.catalyst,
                QStringLiteral("补齐实验记录"),
                QStringLiteral("当前缺少：%1。没有这些条件，后续即使测试更久也很难做可靠横向比较。")
                    .arg(missing.join(QStringLiteral("、"))),
                QStringLiteral("下一次实验开始前把缺失字段写入实验记录，并在整段寿命测试中保持记录口径一致。"),
                score,
                QStringLiteral("依据数据完整性检查；这是提高后续建议可执行性和可复查性的前置动作。")));
        }
    }

    const auto comparisons = pairComparisons(records, analysis);
    for (const auto& comparison : comparisons) {
        if (!comparison.comparable || comparison.sharedTimeHours <= 0.0) continue;
        const double mean = (qAbs(comparison.performanceA) + qAbs(comparison.performanceB)) / 2.0;
        if (mean <= 1.0e-12) continue;
        const double relativeDifference = comparison.absoluteDifference / mean;
        if (relativeDifference <= 0.03) {
            advice.append(makeAdvice(
                AdvicePriority::Normal,
                QStringLiteral("%1 / %2").arg(comparison.catalystA, comparison.catalystB),
                QStringLiteral("先做重复验证再判优"),
                QStringLiteral("共同时间 %1 h 的相对差异仅约 %2%，小差异容易被实验波动放大。")
                    .arg(QString::number(comparison.sharedTimeHours, 'g', 6),
                         QString::number(relativeDifference * 100.0, 'f', 1)),
                QStringLiteral("资源有限时先各补 1 次独立重复；用于正式比较时，优先获得可估计离散度的独立重复并报告误差。"),
                78,
                QStringLiteral("依据当前共同时间点差异小于均值 3% 的保守规则；软件不会把微小数值差直接当成真实优劣。")));
        }
    }

    return advice;
}

QVector<PairComparison> ResearchAdvisor::pairComparisons(
    const QVector<Record>& records,
    const AnalysisResult& analysis) {
    QVector<PairComparison> comparisons;
    QMap<QString, QVector<Record>> grouped;
    for (const auto& record : records) {
        if (!record.catalyst.trimmed().isEmpty()) grouped[record.catalyst.trimmed()].append(record);
    }
    const QStringList names = grouped.keys();
    for (qsizetype i = 0; i < names.size(); ++i) {
        for (qsizetype j = i + 1; j < names.size(); ++j) {
            const QString& a = names[i];
            const QString& b = names[j];
            PairComparison comparison;
            comparison.catalystA = a;
            comparison.catalystB = b;

            std::optional<double> latestShared;
            for (const auto& row : grouped[a]) {
                if (performanceAt(grouped[b], row.timeHours)
                    && (!latestShared || row.timeHours > *latestShared)) {
                    latestShared = row.timeHours;
                }
            }
            if (!latestShared) {
                comparison.status = QStringLiteral("无共同时间点");
                comparisons.append(comparison);
                continue;
            }

            comparison.sharedTimeHours = *latestShared;
            comparison.performanceA = performanceAt(grouped[a], *latestShared).value_or(0.0);
            comparison.performanceB = performanceAt(grouped[b], *latestShared).value_or(0.0);
            comparison.absoluteDifference = qAbs(comparison.performanceA - comparison.performanceB);
            comparison.leader = comparison.performanceA >= comparison.performanceB ? a : b;

            if (pairHasMismatch(analysis.conditionAudit, a, b)) {
                comparison.status = QStringLiteral("条件不一致");
                comparison.comparable = false;
            } else if (analysis.conditionAudit.status == ConditionAuditStatus::ConditionsNotProvided) {
                comparison.status = QStringLiteral("仅供参考");
                comparison.comparable = true;
            } else {
                comparison.status = QStringLiteral("可比较");
                comparison.comparable = true;
            }
            comparisons.append(comparison);
        }
    }
    return comparisons;
}

} // namespace catalyst
