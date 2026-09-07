#pragma once

#include "models.h"

#include <QMainWindow>
#include <QVector>

class QCloseEvent;
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

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void exportReport();
    void importCsv();
    void loadDemo();
    void runAnalysis();

private:
    void buildUi();
    void applyTheme();
    QWidget* buildSidebar();
    QWidget* buildProjectPage();
    QWidget* buildOverviewPage();
    QWidget* buildDataPage();
    QWidget* buildAnalysisPage();
    QWidget* buildAiPage();
    QWidget* buildSettingsPage();
    void setRecords(const QVector<Record>& records, const QString& sourceLabel, bool markDirty = true);
    void refreshRawTable();
    void refreshAnalysisViews();
    void updateProjectUi();
    void setStatus(const QString& text, bool error = false);
    bool saveProjectTo(const QString& path);
    bool confirmProjectTransition();

    QVector<Record> records_;
    AnalysisResult analysis_;
    QString sourceLabelText_;
    QString currentProjectPath_;
    bool projectDirty_ = false;

    QStackedWidget* pages_ = nullptr;
    QLabel* projectPathLabel_ = nullptr;
    QLabel* projectStateLabel_ = nullptr;
    QLabel* sourceLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* metricCatalysts_ = nullptr;
    QLabel* metricPoints_ = nullptr;
    QLabel* metricLongest_ = nullptr;
    QLabel* metricLeader_ = nullptr;
    QLabel* metricCondition_ = nullptr;
    QLabel* conditionStatusLabel_ = nullptr;
    QLabel* conditionMessageLabel_ = nullptr;
    QTableWidget* conditionMismatchTable_ = nullptr;
    QTableWidget* summaryTable_ = nullptr;
    QTableWidget* rawTable_ = nullptr;
    QTableWidget* thresholdTable_ = nullptr;
    ChartWidget* chart_ = nullptr;
};

} // namespace catalyst
