#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace catalyst {

enum class ThresholdStatus {
    LeftCensored,
    Interval,
    RightCensored
};

enum class ConditionAuditStatus {
    NoData,
    ConditionsNotProvided,
    MatchedOnProvidedConditions,
    MismatchDetected
};

struct ThresholdObservation {
    double threshold = 0.0;
    ThresholdStatus status = ThresholdStatus::RightCensored;
    std::optional<double> lowerHours;
    std::optional<double> upperHours;
};

struct Record {
    QString catalyst;
    double timeHours = 0.0;
    double performance = 0.0;
    std::optional<double> temperatureC;
    std::optional<double> gHSV;
    std::optional<double> wHSV;
    std::optional<double> pressure;
    QString feedRatio;
    QString metric;
    QString source;
};

struct PairConditionMismatch {
    QString catalystA;
    QString catalystB;
    QStringList fields;
};

struct ConditionAudit {
    ConditionAuditStatus status = ConditionAuditStatus::NoData;
    QStringList explicitFields;
    QVector<PairConditionMismatch> pairMismatches;
    QString message;

    [[nodiscard]] bool blocksDirectRanking() const {
        return status == ConditionAuditStatus::MismatchDetected && !pairMismatches.isEmpty();
    }
};

struct CatalystSummary {
    QString catalyst;
    int observations = 0;
    double initialTimeHours = 0.0;
    double initialPerformance = 0.0;
    double latestTimeHours = 0.0;
    double latestPerformance = 0.0;
    double retentionPercent = 0.0;
    ThresholdObservation t95;
    ThresholdObservation t90;
    ThresholdObservation t80;
};

struct AnalysisResult {
    QVector<CatalystSummary> catalysts;
    int totalObservations = 0;
    double longestTestHours = 0.0;
    std::optional<double> latestSharedTimeHours;
    QString latestSharedLeader;
    ConditionAudit conditionAudit;
};

} // namespace catalyst
