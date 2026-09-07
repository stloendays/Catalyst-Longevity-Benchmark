#include "analysisengine.h"
#include "csvreader.h"
#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QStringList>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Catalyst Longevity Research"));
    app.setApplicationName(QStringLiteral("Catalyst Longevity Research"));
    app.setStyle(QStringLiteral("Fusion"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));

    if (app.arguments().contains(QStringLiteral("--self-test"))) {
        const auto records = catalyst::CsvReader::demoData();
        const auto result = catalyst::AnalysisEngine::analyze(records);
        if (result.totalObservations != 6 || result.catalysts.size() != 2) {
            return 2;
        }
        if (!result.latestSharedTimeHours.has_value() || result.latestSharedLeader.isEmpty()) {
            return 3;
        }
        if (result.conditionAudit.status != catalyst::ConditionAuditStatus::MatchedOnProvidedConditions) {
            return 4;
        }

        auto mismatched = records;
        for (auto& row : mismatched) {
            if (row.catalyst == QStringLiteral("Catalyst B")) {
                row.temperatureC = 750.0;
            }
        }
        const auto guarded = catalyst::AnalysisEngine::analyze(mismatched);
        if (guarded.conditionAudit.status != catalyst::ConditionAuditStatus::MismatchDetected) {
            return 5;
        }
        if (!guarded.latestSharedLeader.isEmpty()) {
            return 6;
        }
        if (guarded.conditionAudit.pairMismatches.isEmpty()) {
            return 7;
        }
        return 0;
    }

    catalyst::MainWindow window;
    window.show();
    return app.exec();
}
