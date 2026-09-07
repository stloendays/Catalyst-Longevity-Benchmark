#include "analysisengine.h"
#include "csvreader.h"
#include "mainwindow.h"
#include "projectstore.h"

#include <QApplication>
#include <QFont>
#include <QStringList>
#include <QTemporaryDir>

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

        auto mismatched = records;
        for (auto& record : mismatched) {
            if (record.catalyst == QStringLiteral("Catalyst B")) {
                record.temperatureC = 750.0;
            }
        }
        const auto mismatchResult = catalyst::AnalysisEngine::analyze(mismatched);
        if (!mismatchResult.conditionAudit.blocksDirectRanking()
            || !mismatchResult.latestSharedLeader.isEmpty()) {
            return 4;
        }

        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            return 5;
        }
        const QString projectPath = tempDir.filePath(QStringLiteral("self-test.clrproj"));
        QString persistenceMessage;
        if (!catalyst::ProjectStore::saveProject(projectPath, records, &persistenceMessage)) {
            return 6;
        }
        QVector<catalyst::Record> loaded;
        if (!catalyst::ProjectStore::loadProject(projectPath, &loaded, &persistenceMessage)) {
            return 7;
        }
        if (loaded.size() != records.size()
            || loaded.front().catalyst != records.front().catalyst
            || loaded.front().temperatureC != records.front().temperatureC) {
            return 8;
        }
        return 0;
    }

    catalyst::MainWindow window;
    window.show();
    return app.exec();
}
