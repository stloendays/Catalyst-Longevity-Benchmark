#pragma once

#include "models.h"

#include <QMainWindow>
#include <QVector>

class QLabel;
class QStackedWidget;
class QTableWidget;
class QWidget;

namespace catalyst {

class ChartWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void importCsv();
    void loadDemo();
    void runAnalysis();

private:
    void buildUi();
    void applyTheme();
    QWidget* buildSidebar();
    QWidget* buildOverviewPage();
    QWidget* buildDataPage();
    QWidget* buildAnalysisPage();
    QWidget* buildAiPage();
    QWidget* buildSettingsPage();
    void setRecords(const QVector<Record>& records, const QString& sourceLabel);
    void refreshRawTable();
    void refreshAnalysisViews();
    void setStatus(const QString& text, bool error = false);

    QVector<Record> records_;
    AnalysisResult analysis_;
    QString sourceLabelText_;

    QStackedWidget* pages_ = nullptr;
    QLabel* sourceLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* metricCatalysts_ = nullptr;
    QLabel* metricPoints_ = nullptr;
    QLabel* metricLongest_ = nullptr;
    QLabel* metricLeader_ = nullptr;
    QTableWidget* summaryTable_ = nullptr;
    QTableWidget* rawTable_ = nullptr;
    QTableWidget* thresholdTable_ = nullptr;
    ChartWidget* chart_ = nullptr;
};

} // namespace catalyst
