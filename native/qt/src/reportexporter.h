#pragma once

#include "documentanalyzer.h"
#include "models.h"

#include <QString>
#include <QVector>

namespace catalyst {

class ReportExporter {
public:
    static bool exportPdf(
        const QString& path,
        const AnalysisResult& result,
        const QString& sourceLabel,
        QString* errorMessage = nullptr);

    static bool exportPdf(
        const QString& path,
        const AnalysisResult& result,
        const QString& sourceLabel,
        const QVector<EvidenceItem>& evidenceItems,
        QString* errorMessage = nullptr);
};

} // namespace catalyst
