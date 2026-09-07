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
    app.setApplicationVersion(QStringLiteral("0.1.0-native-preview"));
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
        return 0;
    }

    catalyst::MainWindow window;
    window.show();
    return app.exec();
}
