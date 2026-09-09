#pragma once

#include "models.h"

#include <QString>
#include <QVector>

namespace catalyst {

enum class CheckLevel {
    Info,
    Warning,
    Error
};

struct DataCheckItem {
    CheckLevel level = CheckLevel::Info;
    QString scope;
    QString issue;
    QString suggestion;
};

struct DataCheckResult {
    int score = 100;
    int errorCount = 0;
    int warningCount = 0;
    int infoCount = 0;
    QVector<DataCheckItem> items;

    [[nodiscard]] QString statusText() const;
};

enum class AdvicePriority {
    Normal,
    Important,
    High
};

struct ExperimentAdvice {
    AdvicePriority priority = AdvicePriority::Normal;
    QString catalyst;
    QString action;
    QString reason;
    QString target;
};

struct PairComparison {
    QString catalystA;
    QString catalystB;
    bool comparable = false;
    QString status;
    double sharedTimeHours = 0.0;
    double performanceA = 0.0;
    double performanceB = 0.0;
    double absoluteDifference = 0.0;
    QString leader;
};

class ResearchAdvisor final {
public:
    static DataCheckResult checkData(const QVector<Record>& records, const AnalysisResult& analysis);
    static QVector<ExperimentAdvice> experimentAdvice(
        const QVector<Record>& records,
        const AnalysisResult& analysis);
    static QVector<PairComparison> pairComparisons(
        const QVector<Record>& records,
        const AnalysisResult& analysis);

    static QString checkLevelText(CheckLevel level);
    static QString advicePriorityText(AdvicePriority priority);
};

} // namespace catalyst
