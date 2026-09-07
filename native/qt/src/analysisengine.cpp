#include "analysisengine.h"

#include <QMap>
#include <QtMath>
#include <algorithm>
#include <stdexcept>

namespace catalyst {

namespace {

bool sameTime(double a, double b) {
    return qAbs(a - b) < 1.0e-9;
}

std::optional<double> performanceAt(const QVector<Record>& rows, double timeHours) {
    for (const auto& row : rows) {
        if (sameTime(row.timeHours, timeHours)) {
            return row.performance;
        }
    }
    return std::nullopt;
}

} // namespace

ThresholdObservation AnalysisEngine::thresholdObservation(
    const QVector<double>& timeHours,
    const QVector<double>& normalizedActivity,
    double threshold) {
    if (timeHours.isEmpty() || timeHours.size() != normalizedActivity.size()) {
        throw std::invalid_argument("time and activity arrays must have equal non-zero length");
    }
    if (!(threshold > 0.0 && threshold <= 1.0)) {
        throw std::invalid_argument("threshold must be in (0, 1]");
    }
    for (qsizetype i = 1; i < timeHours.size(); ++i) {
        if (timeHours[i - 1] > timeHours[i]) {
            throw std::invalid_argument("observation times must be monotonically increasing");
        }
    }

    ThresholdObservation result;
    result.threshold = threshold;

    if (normalizedActivity.front() <= threshold) {
        result.status = ThresholdStatus::LeftCensored;
        result.lowerHours = 0.0;
        result.upperHours = timeHours.front();
        return result;
    }

    for (qsizetype i = 1; i < timeHours.size(); ++i) {
        if (normalizedActivity[i] <= threshold && threshold <= normalizedActivity[i - 1]) {
            result.status = ThresholdStatus::Interval;
            result.lowerHours = timeHours[i - 1];
            result.upperHours = timeHours[i];
            return result;
        }
    }

    result.status = ThresholdStatus::RightCensored;
    result.lowerHours = timeHours.back();
    result.upperHours = std::nullopt;
    return result;
}

AnalysisResult AnalysisEngine::analyze(const QVector<Record>& records) {
    AnalysisResult result;
    result.totalObservations = records.size();
    if (records.isEmpty()) {
        return result;
    }

    QMap<QString, QVector<Record>> grouped;
    for (const auto& record : records) {
        if (record.catalyst.trimmed().isEmpty()) {
            continue;
        }
        grouped[record.catalyst.trimmed()].append(record);
        result.longestTestHours = qMax(result.longestTestHours, record.timeHours);
    }

    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        auto rows = it.value();
        std::sort(rows.begin(), rows.end(), [](const Record& a, const Record& b) {
            return a.timeHours < b.timeHours;
        });
        grouped[it.key()] = rows;

        if (rows.isEmpty()) {
            continue;
        }
        if (qFuzzyIsNull(rows.front().performance)) {
            throw std::invalid_argument("initial catalyst performance cannot be zero");
        }

        CatalystSummary summary;
        summary.catalyst = it.key();
        summary.observations = rows.size();
        summary.initialTimeHours = rows.front().timeHours;
        summary.initialPerformance = rows.front().performance;
        summary.latestTimeHours = rows.back().timeHours;
        summary.latestPerformance = rows.back().performance;
        summary.retentionPercent = 100.0 * summary.latestPerformance / summary.initialPerformance;

        QVector<double> times;
        QVector<double> normalized;
        times.reserve(rows.size());
        normalized.reserve(rows.size());
        for (const auto& row : rows) {
            times.append(row.timeHours);
            normalized.append(row.performance / summary.initialPerformance);
        }
        summary.t95 = thresholdObservation(times, normalized, 0.95);
        summary.t90 = thresholdObservation(times, normalized, 0.90);
        summary.t80 = thresholdObservation(times, normalized, 0.80);
        result.catalysts.append(summary);
    }

    if (!grouped.isEmpty()) {
        const auto firstRows = grouped.constBegin().value();
        std::optional<double> latestShared;
        for (const auto& candidate : firstRows) {
            bool shared = true;
            for (auto it = std::next(grouped.constBegin()); it != grouped.constEnd(); ++it) {
                if (!performanceAt(it.value(), candidate.timeHours).has_value()) {
                    shared = false;
                    break;
                }
            }
            if (shared && (!latestShared.has_value() || candidate.timeHours > *latestShared)) {
                latestShared = candidate.timeHours;
            }
        }

        if (latestShared.has_value()) {
            QString leader;
            std::optional<double> best;
            for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
                const auto value = performanceAt(it.value(), *latestShared);
                if (value.has_value() && (!best.has_value() || *value > *best)) {
                    best = value;
                    leader = it.key();
                }
            }
            result.latestSharedTimeHours = latestShared;
            result.latestSharedLeader = leader;
        }
    }

    return result;
}

QString AnalysisEngine::thresholdText(const ThresholdObservation& observation) {
    const auto number = [](double value) {
        return QString::number(value, 'g', 8);
    };

    switch (observation.status) {
    case ThresholdStatus::LeftCensored:
        return QStringLiteral("≤ %1 h").arg(number(observation.upperHours.value_or(0.0)));
    case ThresholdStatus::Interval:
        return QStringLiteral("%1–%2 h")
            .arg(number(observation.lowerHours.value_or(0.0)), number(observation.upperHours.value_or(0.0)));
    case ThresholdStatus::RightCensored:
        return QStringLiteral("> %1 h").arg(number(observation.lowerHours.value_or(0.0)));
    }
    return QStringLiteral("—");
}

} // namespace catalyst
