#pragma once

#include "models.h"

#include <QString>

namespace catalyst {

class ReportExporter {
public:
    static bool exportPdf(
        const QString& path,
        const AnalysisResult& result,
        const QString& sourceLabel,
        QString* errorMessage = nullptr);
};

} // namespace catalyst
