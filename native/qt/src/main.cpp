#include "analysisengine.h"
#include "csvreader.h"
#include "documentanalyzer.h"
#include "mainwindow.h"
#include "projectstore.h"
#include "reportexporter.h"

#include "xlsxdocument.h"

#include <QApplication>
#include <QFileInfo>
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

        const QString reportPath = tempDir.filePath(QStringLiteral("self-test-report.pdf"));
        QString reportMessage;
        if (!catalyst::ReportExporter::exportPdf(
                reportPath, result, QStringLiteral("self-test"), &reportMessage)) {
            return 9;
        }
        if (!QFileInfo::exists(reportPath) || QFileInfo(reportPath).size() <= 0) {
            return 10;
        }

        const QString workbookPath = tempDir.filePath(QStringLiteral("self-test.xlsx"));
        {
            QXlsx::Document workbook;
            workbook.write(1, 1, QStringLiteral("催化剂"));
            workbook.write(1, 2, QStringLiteral("时间"));
            workbook.write(1, 3, QStringLiteral("性能"));
            workbook.write(1, 4, QStringLiteral("温度"));
            workbook.write(2, 1, QStringLiteral("Excel Catalyst"));
            workbook.write(2, 2, 0.0);
            workbook.write(2, 3, 81.0);
            workbook.write(2, 4, 650.0);
            workbook.write(3, 1, QStringLiteral("Excel Catalyst"));
            workbook.write(3, 2, 24.0);
            workbook.write(3, 3, 76.0);
            workbook.write(3, 4, 650.0);
            if (!workbook.saveAs(workbookPath)) {
                return 11;
            }
        }

        QString importMessage;
        const auto workbookRecords = catalyst::CsvReader::readFile(workbookPath, &importMessage);
        if (workbookRecords.size() != 2
            || workbookRecords.front().catalyst != QStringLiteral("Excel Catalyst")
            || !workbookRecords.front().temperatureC.has_value()
            || *workbookRecords.front().temperatureC != 650.0) {
            return 12;
        }

        const QString evidenceText = QStringLiteral(
            "DOI 10.1234/example.2026.42. Catalyst X was tested at 700 °C for 20 h. "
            "CH4 conversion remained 82%. Long-term stability was limited by coking and sintering, "
            "followed by regeneration.");
        const auto documentSignals = catalyst::DocumentAnalyzer::analyzeText(
            evidenceText, QStringLiteral("self-test.txt"));
        if (documentSignals.dois.isEmpty()
            || documentSignals.temperaturesC.isEmpty()
            || documentSignals.durationsHours.isEmpty()
            || documentSignals.ch4ConversionPercentCandidates.isEmpty()
            || !documentSignals.keywordEvidence.contains(QStringLiteral("stability"))
            || documentSignals.snippets.isEmpty()) {
            return 13;
        }

        return 0;
    }

    catalyst::MainWindow window;
    window.show();
    return app.exec();
}
