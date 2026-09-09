#include "researchadvisor.h"

#include <QMap>
#include <QSet>
#include <QtMath>
#include <algorithm>

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

double suggestedExtension(const QVector<Record>& inputRows) {
    if (inputRows.isEmpty()) return 0.0;
    QVector<Record> rows = inputRows;
    std::sort(rows.begin(), rows.end(), [](const Record& a, const Record& b) {
        return a.timeHours < b.timeHours;
    });

    const double latest = rows.back().timeHours;
    QVector<double> deltas;
    for (qsizetype i = 1; i < rows.size(); ++i) {
        const double delta = rows[i].timeHours - rows[i - 1].timeHours;
        if (delta > 1.0e-9) deltas.append(delta);
    }
    double typicalStep = qMax(1.0, latest * 0.20);
    if (!deltas.isEmpty()) {
        std::sort(deltas.begin(), deltas.end());
        typicalStep = deltas[deltas.size() / 2];
    }
    return qMax(latest * 1.25, latest + 2.0 * typicalStep);
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
    const AnalysisResult& analysis) {
    QVector<ExperimentAdvice> advice;
    if (records.isEmpty()) {
        advice.append({AdvicePriority::High, QStringLiteral("全部"),
            QStringLiteral("先导入实验数据"),
            QStringLiteral("当前没有可用于寿命分析的记录。"),
            QStringLiteral("建议每个催化剂至少准备 3 个时间点。")});
        return advice;
    }

    QMap<QString, QVector<Record>> grouped;
    for (const auto& record : records) grouped[record.catalyst.trimmed()].append(record);

    if (analysis.conditionAudit.blocksDirectRanking()) {
        advice.append({AdvicePriority::High, QStringLiteral("全部"),
            QStringLiteral("统一比较条件"),
            QStringLiteral("当前不同催化剂之间存在明确实验条件差异。"),
            QStringLiteral("优先统一温度、空速、压力和进料条件，再做横向比较。")});
    }

    for (const auto& summary : analysis.catalysts) {
        const auto rows = grouped.value(summary.catalyst);
        if (summary.observations < 3) {
            advice.append({AdvicePriority::High, summary.catalyst,
                QStringLiteral("补充时间点"),
                QStringLiteral("当前只有 %1 个观测点，趋势判断较弱。\n").arg(summary.observations).trimmed(),
                QStringLiteral("建议至少补到 3 个时间点。")});
        }

        if (summary.t90.status == ThresholdStatus::RightCensored) {
            const double target = suggestedExtension(rows);
            advice.append({AdvicePriority::Important, summary.catalyst,
                QStringLiteral("延长寿命测试"),
                QStringLiteral("测试结束时仍未达到 T90，目前只能得到寿命下限。"),
                QStringLiteral("下一轮可优先延长到约 %1 h，并保留中间采样点。")
                    .arg(QString::number(target, 'g', 6))});
        } else if (summary.t90.status == ThresholdStatus::Interval
                   && summary.t90.lowerHours && summary.t90.upperHours) {
            const double midpoint = (*summary.t90.lowerHours + *summary.t90.upperHours) / 2.0;
            advice.append({AdvicePriority::Important, summary.catalyst,
                QStringLiteral("加密 T90 附近采样"),
                QStringLiteral("T90 目前位于 %1–%2 h 区间。")
                    .arg(QString::number(*summary.t90.lowerHours, 'g', 6),
                         QString::number(*summary.t90.upperHours, 'g', 6)),
                QStringLiteral("下一轮可在约 %1 h 增加一个采样点，缩小寿命区间。")
                    .arg(QString::number(midpoint, 'g', 6))});
        } else if (summary.t90.status == ThresholdStatus::LeftCensored) {
            const double upper = summary.t90.upperHours.value_or(summary.initialTimeHours);
            advice.append({AdvicePriority::High, summary.catalyst,
                QStringLiteral("提前首个采样点"),
                QStringLiteral("第一次观测时已经达到 T90，寿命发生在更早时间。"),
                upper > 0.0
                    ? QStringLiteral("下一轮可尝试在 %1 h 之前增加采样点。")
                        .arg(QString::number(upper * 0.5, 'g', 6))
                    : QStringLiteral("下一轮应增加更早的采样点。")});
        }

        const QStringList missing = missingConditionFields(rows);
        if (!missing.isEmpty()) {
            advice.append({AdvicePriority::Normal, summary.catalyst,
                QStringLiteral("补全实验记录"),
                QStringLiteral("当前缺少：%1。\n").arg(missing.join(QStringLiteral("、"))).trimmed(),
                QStringLiteral("补全后可以提高跨催化剂比较和报告复用的可靠性。")});
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
