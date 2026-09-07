#pragma once

#include "models.h"

#include <QString>
#include <QVector>

namespace catalyst {

class AnalysisEngine {
public:
    static AnalysisResult analyze(const QVector<Record>& records);
    static ThresholdObservation thresholdObservation(
        const QVector<double>& timeHours,
        const QVector<double>& normalizedActivity,
        double threshold);
    static QString thresholdText(const ThresholdObservation& observation);
};

} // namespace catalyst
