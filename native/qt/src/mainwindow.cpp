#include "mainwindow.h"

#include "analysisengine.h"
#include "chartwidget.h"
#include "conditionguard.h"
#include "csvreader.h"
#include "evidencepage.h"
#include "projectstore.h"
#include "reportexporter.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>

namespace catalyst {

namespace {

QLabel* heading(const QString& text, int pointSize = 20) {
    auto* label = new QLabel(text);
    label->setFont(QFont(QStringLiteral("Microsoft YaHei UI"), pointSize, QFont::DemiBold));
    label->setObjectName(QStringLiteral("pageHeading"));
    return label;
}

QLabel* muted(const QString& text) {
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setObjectName(QStringLiteral("mutedText"));
    return label;
}

QFrame* metricCard(const QString& title, QLabel** valueLabel) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("metricCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("metricTitle"));
    auto* value = new QLabel(QStringLiteral("—"));
    value->setObjectName(QStringLiteral("metricValue"));
    value->setWordWrap(true);
    layout->addWidget(titleLabel);
    layout->addWidget(value);
    *valueLabel = value;
    return card;
}

QTableWidgetItem* readOnlyItem(const QString& text) {
    auto* cell = new QTableWidgetItem(text);
    cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
    return cell;
}

QString numberOrDash(const std::optional<double>& value) {
    return value.has_value() ? QString::number(*value, 'g', 8) : QStringLiteral("—");
}

QString conditionFieldLabel(const QString& field) {
    if (field == QStringLiteral("temperature_c")) return QStringLiteral("温度");
    if (field == QStringLiteral("ghsv")) return QStringLiteral("GHSV");
    if (field == QStringLiteral("whsv")) return QStringLiteral("WHSV");
    if (field == QStringLiteral("pressure_bar")) return QStringLiteral("压力");
    if (field == QStringLiteral("feed_ratio")) return QStringLiteral("进料比");
    return field;
}

QString conditionFieldsText(const QStringList& fields) {
    QStringList labels;
    labels.reserve(fields.size());
    for (const auto& field : fields) {
        labels.append(conditionFieldLabel(field));
    }
    return labels.join(QStringLiteral("、"));
}

void configureTable(QTableWidget* table) {
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    resize(1380, 860);
    setMinimumSize(1120, 720);
    buildUi();
    applyTheme();
    newProject();
}

void MainWindow::buildUi() {
    auto* central = new QWidget;
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(buildSidebar());

    pages_ = new QStackedWidget;
    pages_->addWidget(buildProjectPage());       // 0
    pages_->addWidget(buildOverviewPage());      // 1
    pages_->addWidget(buildDataPage());          // 2
    pages_->addWidget(new EvidencePage);          // 3
    pages_->addWidget(buildAnalysisPage());      // 4
    pages_->addWidget(buildAiPage());            // 5
    pages_->addWidget(buildSettingsPage());      // 6
    root->addWidget(pages_, 1);

    setCentralWidget(central);
    statusLabel_ = new QLabel(QStringLiteral("Ready"));
    statusBar()->addPermanentWidget(statusLabel_, 1);
}

QWidget* MainWindow::buildSidebar() {
    auto* sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(236);
    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(18, 24, 18, 18);
    layout->setSpacing(8);

    auto* brand = new QLabel(QStringLiteral("CATALYST\nLONGEVITY"));
    brand->setObjectName(QStringLiteral("brand"));
    layout->addWidget(brand);
    layout->addSpacing(22);

    auto* group = new QButtonGroup(sidebar);
    group->setExclusive(true);
    const QStringList labels = {
        QStringLiteral("项目"),
        QStringLiteral("总览"),
        QStringLiteral("数据导入"),
        QStringLiteral("资料分析"),
        QStringLiteral("寿命分析"),
        QStringLiteral("AI 工作区"),
        QStringLiteral("设置")
    };

    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QPushButton(labels[i]);
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCursor(Qt::PointingHandCursor);
        group->addButton(button, i);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i]() {
            pages_->setCurrentIndex(i);
        });
        if (i == 0) button->setChecked(true);
    }

    layout->addStretch();
    auto* buildLabel = new QLabel(QStringLiteral("Native Desktop\nC++ / Qt 6"));
    buildLabel->setObjectName(QStringLiteral("sidebarFoot"));
    layout->addWidget(buildLabel);
    return sidebar;
}

QWidget* MainWindow::buildProjectPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 28, 34, 28);
    layout->setSpacing(18);
    layout->addWidget(heading(QStringLiteral("项目工作区")));
    layout->addWidget(muted(QStringLiteral(
        "项目文件使用本地 SQLite 保存实验记录与项目元数据。关闭软件后可以直接重新打开 .clrproj 继续分析。")));

    auto* actions = new QHBoxLayout;
    const auto addAction = [this, actions](const QString& text, const char* style, auto slot) {
        auto* button = new QPushButton(text);
        button->setObjectName(QString::fromLatin1(style));
        connect(button, &QPushButton::clicked, this, slot);
        actions->addWidget(button);
    };
    addAction(QStringLiteral("新建项目"), "secondaryButton", &MainWindow::newProject);
    addAction(QStringLiteral("打开项目"), "secondaryButton", &MainWindow::openProject);
    addAction(QStringLiteral("保存"), "primaryButton", &MainWindow::saveProject);
    addAction(QStringLiteral("另存为"), "secondaryButton", &MainWindow::saveProjectAs);
    actions->addStretch();
    layout->addLayout(actions);

    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("panel"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->setSpacing(10);
    auto* title = new QLabel(QStringLiteral("当前项目"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    projectPathLabel_ = new QLabel(QStringLiteral("未命名项目"));
    projectPathLabel_->setObjectName(QStringLiteral("sourcePath"));
    projectPathLabel_->setWordWrap(true);
    projectPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    projectStateLabel_ = muted(QStringLiteral("尚未保存"));
    cardLayout->addWidget(title);
    cardLayout->addWidget(projectPathLabel_);
    cardLayout->addWidget(projectStateLabel_);
    cardLayout->addSpacing(12);
    cardLayout->addWidget(muted(QStringLiteral(
        ".clrproj 内部为 SQLite 数据库，当前保存催化剂时间序列、实验条件、指标与数据来源。")));
    cardLayout->addStretch();
    layout->addWidget(card, 1);
    return page;
}

QWidget* MainWindow::buildOverviewPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 28, 34, 28);
    layout->setSpacing(18);

    auto* top = new QHBoxLayout;
    auto* titleBox = new QVBoxLayout;
    titleBox->addWidget(heading(QStringLiteral("催化剂长期表现总览")));
    titleBox->addWidget(muted(QStringLiteral(
        "导入 CSV 或 Excel 后，直接计算保持率、删失感知寿命阈值与实验条件守门结果。")));
    top->addLayout(titleBox, 1);

    auto* demoButton = new QPushButton(QStringLiteral("载入示例"));
    auto* importButton = new QPushButton(QStringLiteral("导入数据"));
    auto* analyzeButton = new QPushButton(QStringLiteral("重新分析"));
    auto* exportButton = new QPushButton(QStringLiteral("导出 PDF"));
    demoButton->setObjectName(QStringLiteral("secondaryButton"));
    importButton->setObjectName(QStringLiteral("primaryButton"));
    analyzeButton->setObjectName(QStringLiteral("primaryButton"));
    exportButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(demoButton, &QPushButton::clicked, this, &MainWindow::loadDemo);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importCsv);
    connect(analyzeButton, &QPushButton::clicked, this, &MainWindow::runAnalysis);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportReport);
    top->addWidget(demoButton);
    top->addWidget(importButton);
    top->addWidget(analyzeButton);
    top->addWidget(exportButton);
    layout->addLayout(top);

    auto* metrics = new QGridLayout;
    metrics->setHorizontalSpacing(12);
    metrics->addWidget(metricCard(QStringLiteral("催化剂"), &metricCatalysts_), 0, 0);
    metrics->addWidget(metricCard(QStringLiteral("数据点"), &metricPoints_), 0, 1);
    metrics->addWidget(metricCard(QStringLiteral("最长测试"), &metricLongest_), 0, 2);
    metrics->addWidget(metricCard(QStringLiteral("条件守门"), &metricCondition_), 0, 3);
    metrics->addWidget(metricCard(QStringLiteral("共同时间领先"), &metricLeader_), 0, 4);
    layout->addLayout(metrics);

    auto* chartFrame = new QFrame;
    chartFrame->setObjectName(QStringLiteral("panel"));
    auto* chartLayout = new QVBoxLayout(chartFrame);
    chartLayout->setContentsMargins(18, 16, 18, 16);
    auto* chartTitle = new QLabel(QStringLiteral("性能随时间变化"));
    chartTitle->setObjectName(QStringLiteral("sectionTitle"));
    chart_ = new ChartWidget;
    chartLayout->addWidget(chartTitle);
    chartLayout->addWidget(chart_, 1);
    layout->addWidget(chartFrame, 1);

    auto* tableFrame = new QFrame;
    tableFrame->setObjectName(QStringLiteral("panel"));
    auto* tableLayout = new QVBoxLayout(tableFrame);
    tableLayout->setContentsMargins(18, 16, 18, 16);
    auto* tableTitle = new QLabel(QStringLiteral("催化剂概览"));
    tableTitle->setObjectName(QStringLiteral("sectionTitle"));
    tableLayout->addWidget(tableTitle);
    summaryTable_ = new QTableWidget(0, 7);
    summaryTable_->setHorizontalHeaderLabels({
        QStringLiteral("催化剂"), QStringLiteral("观测点"), QStringLiteral("初始性能"),
        QStringLiteral("最新性能"), QStringLiteral("保持率"), QStringLiteral("最新时间"),
        QStringLiteral("T90")});
    configureTable(summaryTable_);
    tableLayout->addWidget(summaryTable_);
    layout->addWidget(tableFrame);
    return page;
}

QWidget* MainWindow::buildDataPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 28, 34, 28);
    layout->setSpacing(16);

    auto* top = new QHBoxLayout;
    auto* titleBox = new QVBoxLayout;
    titleBox->addWidget(heading(QStringLiteral("数据导入")));
    titleBox->addWidget(muted(QStringLiteral(
        "支持 CSV 与 Excel .xlsx。至少包含：催化剂、时间、性能；Excel 会自动扫描工作表并选择含必需列的工作表。")));
    top->addLayout(titleBox, 1);
    auto* importButton = new QPushButton(QStringLiteral("选择 CSV / Excel"));
    importButton->setObjectName(QStringLiteral("primaryButton"));
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importCsv);
    top->addWidget(importButton);
    layout->addLayout(top);

    auto* sourceFrame = new QFrame;
    sourceFrame->setObjectName(QStringLiteral("panel"));
    auto* sourceLayout = new QVBoxLayout(sourceFrame);
    sourceLayout->setContentsMargins(18, 14, 18, 14);
    sourceLayout->addWidget(new QLabel(QStringLiteral("当前数据源")));
    sourceLabel_ = new QLabel(QStringLiteral("—"));
    sourceLabel_->setObjectName(QStringLiteral("sourcePath"));
    sourceLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sourceLabel_->setWordWrap(true);
    sourceLayout->addWidget(sourceLabel_);
    layout->addWidget(sourceFrame);

    rawTable_ = new QTableWidget(0, 9);
    rawTable_->setHorizontalHeaderLabels({
        QStringLiteral("催化剂"), QStringLiteral("时间(h)"), QStringLiteral("性能"),
        QStringLiteral("温度"), QStringLiteral("GHSV"), QStringLiteral("WHSV"),
        QStringLiteral("压力"), QStringLiteral("进料比"), QStringLiteral("指标")});
    configureTable(rawTable_);
    layout->addWidget(rawTable_, 1);
    return page;
}

QWidget* MainWindow::buildAnalysisPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 28, 34, 28);
    layout->setSpacing(16);
    layout->addWidget(heading(QStringLiteral("寿命与条件可比性分析")));
    layout->addWidget(muted(QStringLiteral(
        "T95 / T90 / T80 保留离散观测的删失语义；直接跨催化剂结论同时受实验条件守门约束。")));

    auto* guardFrame = new QFrame;
    guardFrame->setObjectName(QStringLiteral("panel"));
    auto* guardLayout = new QVBoxLayout(guardFrame);
    guardLayout->setContentsMargins(18, 16, 18, 16);
    auto* guardTitle = new QLabel(QStringLiteral("实验条件守门"));
    guardTitle->setObjectName(QStringLiteral("sectionTitle"));
    conditionStatusLabel_ = new QLabel(QStringLiteral("—"));
    conditionStatusLabel_->setObjectName(QStringLiteral("guardStatus"));
    conditionMessageLabel_ = muted(QStringLiteral("尚未分析。"));
    guardLayout->addWidget(guardTitle);
    guardLayout->addWidget(conditionStatusLabel_);
    guardLayout->addWidget(conditionMessageLabel_);

    conditionMismatchTable_ = new QTableWidget(0, 3);
    conditionMismatchTable_->setHorizontalHeaderLabels({
        QStringLiteral("催化剂 A"), QStringLiteral("催化剂 B"), QStringLiteral("不匹配条件")});
    configureTable(conditionMismatchTable_);
    conditionMismatchTable_->setMaximumHeight(180);
    guardLayout->addWidget(conditionMismatchTable_);
    layout->addWidget(guardFrame);

    thresholdTable_ = new QTableWidget(0, 7);
    thresholdTable_->setHorizontalHeaderLabels({
        QStringLiteral("催化剂"), QStringLiteral("保持率"), QStringLiteral("T95"),
        QStringLiteral("T90"), QStringLiteral("T80"), QStringLiteral("初始值"), QStringLiteral("最新值")});
    configureTable(thresholdTable_);
    layout->addWidget(thresholdTable_, 1);

    auto* note = new QFrame;
    note->setObjectName(QStringLiteral("infoPanel"));
    auto* noteLayout = new QVBoxLayout(note);
    noteLayout->addWidget(new QLabel(QStringLiteral("证据语义")));
    noteLayout->addWidget(muted(QStringLiteral(
        "例如 T90 = 20–50 h 表示首次通过阈值只被观测数据约束在该区间；不会线性插值成伪精确寿命。若温度、空速、压力或进料比明确不一致，也不会输出直接领先者。")));
    layout->addWidget(note);
    return page;
}

QWidget* MainWindow::buildAiPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 28, 34, 28);
    layout->setSpacing(16);
    layout->addWidget(heading(QStringLiteral("AI 工作区")));
    layout->addWidget(muted(QStringLiteral(
        "原生资料分析后端已进入桌面版。下一阶段将把 Evidence Packet、AI Analyst、Evidence Critic 与外部数据库客户端接入这里。")));

    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("panel"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 22, 24, 22);
    auto* title = new QLabel(QStringLiteral("Native AI integration roadmap"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    cardLayout->addWidget(title);
    cardLayout->addWidget(muted(QStringLiteral(
        "Evidence Packet  ·  HTTP/API 客户端  ·  AI Analyst  ·  Evidence Critic  ·  审计日志")));
    cardLayout->addStretch();
    layout->addWidget(card, 1);
    return page;
}

QWidget* MainWindow::buildSettingsPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 28, 34, 28);
    layout->setSpacing(16);
    layout->addWidget(heading(QStringLiteral("设置与软件信息")));

    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("panel"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->addWidget(new QLabel(QStringLiteral("Catalyst Longevity Research")));
    cardLayout->addWidget(muted(QStringLiteral("原生 Windows 桌面版 · C++20 + Qt 6 Widgets + SQLite")));
    cardLayout->addSpacing(10);
    cardLayout->addWidget(new QLabel(QStringLiteral("运行方式：本地桌面窗口，不启动浏览器，不依赖 Streamlit。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("数据输入：CSV / Excel .xlsx。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("项目存储：本地 .clrproj SQLite 文件。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("报告输出：原生 PDF 分析报告。")));
    cardLayout->addStretch();
    layout->addWidget(card, 1);
    return page;
}

void MainWindow::applyTheme() {
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background: #F4F6F8; color: #17202A; font-family: "Microsoft YaHei UI"; font-size: 13px; }
        QStatusBar { background: #FFFFFF; border-top: 1px solid #E5E7EB; }
        #sidebar { background: #111827; }
        #brand { color: #FFFFFF; font-size: 18px; font-weight: 700; letter-spacing: 1px; }
        #sidebarFoot { color: #9CA3AF; font-size: 11px; }
        #navButton { color: #D1D5DB; background: transparent; border: none; border-radius: 8px; padding: 11px 13px; text-align: left; font-weight: 500; }
        #navButton:hover { background: #1F2937; color: #FFFFFF; }
        #navButton:checked { background: #2563EB; color: #FFFFFF; }
        #pageHeading { color: #111827; }
        #mutedText { color: #6B7280; }
        #metricCard, #panel { background: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 10px; }
        #infoPanel { background: #EFF6FF; border: 1px solid #BFDBFE; border-radius: 10px; }
        #metricTitle { color: #6B7280; font-size: 12px; }
        #metricValue { color: #111827; font-size: 19px; font-weight: 700; }
        #sectionTitle { color: #111827; font-size: 15px; font-weight: 700; }
        #sourcePath { color: #1D4ED8; }
        #guardStatus { font-size: 15px; font-weight: 700; padding: 3px 0; }
        #primaryButton { background: #2563EB; color: white; border: none; border-radius: 8px; padding: 10px 17px; font-weight: 600; }
        #primaryButton:hover { background: #1D4ED8; }
        #secondaryButton { background: #FFFFFF; color: #374151; border: 1px solid #D1D5DB; border-radius: 8px; padding: 10px 17px; font-weight: 600; }
        #secondaryButton:hover { background: #F9FAFB; }
        QTableWidget { background: #FFFFFF; alternate-background-color: #F9FAFB; border: 1px solid #E5E7EB; border-radius: 8px; gridline-color: #EEF0F2; }
        QHeaderView::section { background: #F3F4F6; color: #374151; border: none; border-bottom: 1px solid #E5E7EB; padding: 8px; font-weight: 600; }
        QTableWidget::item { padding: 6px; }
        QTableWidget::item:selected { background: #DBEAFE; color: #111827; }
    )"));
}

void MainWindow::newProject() {
    if (!confirmProjectTransition()) return;
    currentProjectPath_.clear();
    projectDirty_ = false;
    records_.clear();
    sourceLabelText_ = QStringLiteral("未加载数据");
    analysis_ = AnalysisResult{};
    if (sourceLabel_) sourceLabel_->setText(sourceLabelText_);
    refreshRawTable();
    refreshAnalysisViews();
    updateProjectUi();
    setStatus(QStringLiteral("已新建空白项目。"));
}

void MainWindow::openProject() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开 Catalyst Longevity Research 项目"),
        QString(),
        QStringLiteral("Catalyst Longevity 项目 (*.clrproj);;所有文件 (*.*)"));
    if (path.isEmpty() || !confirmProjectTransition()) return;

    QVector<Record> loaded;
    QString message;
    if (!ProjectStore::loadProject(path, &loaded, &message)) {
        QMessageBox::warning(this, QStringLiteral("打开项目失败"), message);
        setStatus(message, true);
        return;
    }

    currentProjectPath_ = path;
    setRecords(loaded, path, false);
    projectDirty_ = false;
    updateProjectUi();
    setStatus(message);
    pages_->setCurrentIndex(1);
}

void MainWindow::saveProject() {
    if (currentProjectPath_.isEmpty()) {
        saveProjectAs();
        return;
    }
    saveProjectTo(currentProjectPath_);
}

void MainWindow::saveProjectAs() {
    QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存 Catalyst Longevity Research 项目"),
        currentProjectPath_.isEmpty() ? QStringLiteral("Catalyst-Longevity-Research.clrproj") : currentProjectPath_,
        QStringLiteral("Catalyst Longevity 项目 (*.clrproj)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".clrproj");
    saveProjectTo(path);
}

bool MainWindow::saveProjectTo(const QString& path) {
    QString message;
    if (!ProjectStore::saveProject(path, records_, &message)) {
        QMessageBox::warning(this, QStringLiteral("保存项目失败"), message);
        setStatus(message, true);
        return false;
    }
    currentProjectPath_ = path;
    projectDirty_ = false;
    updateProjectUi();
    setStatus(message);
    return true;
}

void MainWindow::exportReport() {
    if (analysis_.catalysts.isEmpty()) {
        const QString message = QStringLiteral("没有可导出的分析结果。请先导入并分析数据。");
        QMessageBox::information(this, QStringLiteral("导出报告"), message);
        setStatus(message, true);
        return;
    }

    const QString suggested = currentProjectPath_.isEmpty()
        ? QStringLiteral("Catalyst-Longevity-Analysis-Report.pdf")
        : QStringLiteral("%1-Analysis-Report.pdf").arg(QFileInfo(currentProjectPath_).completeBaseName());
    QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出分析 PDF"),
        suggested,
        QStringLiteral("PDF 报告 (*.pdf)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".pdf");

    QString message;
    if (!ReportExporter::exportPdf(path, analysis_, sourceLabelText_, &message)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), message);
        setStatus(message, true);
        return;
    }
    setStatus(message);
}

bool MainWindow::confirmProjectTransition() {
    if (!projectDirty_) return true;

    const auto choice = QMessageBox::warning(
        this,
        QStringLiteral("项目有未保存更改"),
        QStringLiteral("当前项目有未保存更改。是否先保存再继续？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Discard) return true;

    if (currentProjectPath_.isEmpty()) {
        QString path = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("保存当前项目"),
            QStringLiteral("Catalyst-Longevity-Research.clrproj"),
            QStringLiteral("Catalyst Longevity 项目 (*.clrproj)"));
        if (path.isEmpty()) return false;
        if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".clrproj");
        return saveProjectTo(path);
    }
    return saveProjectTo(currentProjectPath_);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmProjectTransition()) event->accept();
    else event->ignore();
}

void MainWindow::importCsv() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择催化剂长期表现数据"),
        QString(),
        QStringLiteral("数据文件 (*.csv *.xlsx);;CSV 文件 (*.csv);;Excel 文件 (*.xlsx);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    QString message;
    const auto records = CsvReader::readFile(path, &message);
    if (records.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), message);
        setStatus(message, true);
        return;
    }
    setRecords(records, path, true);
    setStatus(message);
}

void MainWindow::loadDemo() {
    setRecords(CsvReader::demoData(), QStringLiteral("内置示例数据"), true);
    setStatus(QStringLiteral("已载入示例数据。"));
}

void MainWindow::setRecords(const QVector<Record>& records, const QString& sourceLabel, bool markDirty) {
    records_ = records;
    sourceLabelText_ = sourceLabel;
    if (markDirty) projectDirty_ = true;
    if (sourceLabel_) sourceLabel_->setText(sourceLabelText_);
    refreshRawTable();
    if (records_.isEmpty()) {
        analysis_ = AnalysisResult{};
        refreshAnalysisViews();
    } else {
        runAnalysis();
    }
    updateProjectUi();
}

void MainWindow::runAnalysis() {
    if (records_.isEmpty()) {
        analysis_ = AnalysisResult{};
        refreshAnalysisViews();
        setStatus(QStringLiteral("请先导入数据。"), true);
        return;
    }

    try {
        analysis_ = AnalysisEngine::analyze(records_);
    } catch (const std::exception& error) {
        const QString message = QStringLiteral("分析失败：%1").arg(QString::fromUtf8(error.what()));
        QMessageBox::warning(this, QStringLiteral("分析失败"), message);
        setStatus(message, true);
        return;
    }
    refreshAnalysisViews();
    setStatus(QStringLiteral("分析完成：%1 个催化剂，%2 个观测点。")
                  .arg(analysis_.catalysts.size())
                  .arg(analysis_.totalObservations));
}

void MainWindow::refreshRawTable() {
    if (!rawTable_) return;
    rawTable_->setRowCount(records_.size());
    for (qsizetype row = 0; row < records_.size(); ++row) {
        const auto& record = records_[row];
        rawTable_->setItem(row, 0, readOnlyItem(record.catalyst));
        rawTable_->setItem(row, 1, readOnlyItem(QString::number(record.timeHours, 'g', 8)));
        rawTable_->setItem(row, 2, readOnlyItem(QString::number(record.performance, 'g', 8)));
        rawTable_->setItem(row, 3, readOnlyItem(numberOrDash(record.temperatureC)));
        rawTable_->setItem(row, 4, readOnlyItem(numberOrDash(record.gHSV)));
        rawTable_->setItem(row, 5, readOnlyItem(numberOrDash(record.wHSV)));
        rawTable_->setItem(row, 6, readOnlyItem(numberOrDash(record.pressure)));
        rawTable_->setItem(row, 7, readOnlyItem(record.feedRatio.isEmpty() ? QStringLiteral("—") : record.feedRatio));
        rawTable_->setItem(row, 8, readOnlyItem(record.metric.isEmpty() ? QStringLiteral("—") : record.metric));
    }
}

void MainWindow::refreshAnalysisViews() {
    if (!metricCatalysts_) return;

    metricCatalysts_->setText(QString::number(analysis_.catalysts.size()));
    metricPoints_->setText(QString::number(analysis_.totalObservations));
    metricLongest_->setText(analysis_.totalObservations > 0
        ? QStringLiteral("%1 h").arg(QString::number(analysis_.longestTestHours, 'g', 8))
        : QStringLiteral("—"));
    metricCondition_->setText(ConditionGuard::statusText(analysis_.conditionAudit.status));

    if (analysis_.conditionAudit.blocksDirectRanking()) {
        metricLeader_->setText(QStringLiteral("已阻止"));
    } else if (analysis_.latestSharedTimeHours.has_value() && !analysis_.latestSharedLeader.isEmpty()) {
        metricLeader_->setText(QStringLiteral("%1 @ %2 h")
            .arg(analysis_.latestSharedLeader, QString::number(*analysis_.latestSharedTimeHours, 'g', 8)));
    } else {
        metricLeader_->setText(QStringLiteral("—"));
    }

    if (chart_) chart_->setRecords(records_);

    if (conditionStatusLabel_) {
        conditionStatusLabel_->setText(ConditionGuard::statusText(analysis_.conditionAudit.status));
        conditionStatusLabel_->setStyleSheet(analysis_.conditionAudit.blocksDirectRanking()
            ? QStringLiteral("color: #B91C1C; font-size: 15px; font-weight: 700; padding: 3px 0;")
            : QStringLiteral("color: #166534; font-size: 15px; font-weight: 700; padding: 3px 0;"));
    }
    if (conditionMessageLabel_) {
        conditionMessageLabel_->setText(analysis_.conditionAudit.message.isEmpty()
            ? QStringLiteral("尚未提供可审计数据。")
            : analysis_.conditionAudit.message);
    }
    if (conditionMismatchTable_) {
        conditionMismatchTable_->setRowCount(analysis_.conditionAudit.pairMismatches.size());
        for (qsizetype row = 0; row < analysis_.conditionAudit.pairMismatches.size(); ++row) {
            const auto& mismatch = analysis_.conditionAudit.pairMismatches[row];
            conditionMismatchTable_->setItem(row, 0, readOnlyItem(mismatch.catalystA));
            conditionMismatchTable_->setItem(row, 1, readOnlyItem(mismatch.catalystB));
            conditionMismatchTable_->setItem(row, 2, readOnlyItem(conditionFieldsText(mismatch.fields)));
        }
    }

    summaryTable_->setRowCount(analysis_.catalysts.size());
    thresholdTable_->setRowCount(analysis_.catalysts.size());
    for (qsizetype row = 0; row < analysis_.catalysts.size(); ++row) {
        const auto& summary = analysis_.catalysts[row];
        summaryTable_->setItem(row, 0, readOnlyItem(summary.catalyst));
        summaryTable_->setItem(row, 1, readOnlyItem(QString::number(summary.observations)));
        summaryTable_->setItem(row, 2, readOnlyItem(QString::number(summary.initialPerformance, 'g', 8)));
        summaryTable_->setItem(row, 3, readOnlyItem(QString::number(summary.latestPerformance, 'g', 8)));
        summaryTable_->setItem(row, 4, readOnlyItem(QStringLiteral("%1%").arg(QString::number(summary.retentionPercent, 'f', 1))));
        summaryTable_->setItem(row, 5, readOnlyItem(QStringLiteral("%1 h").arg(QString::number(summary.latestTimeHours, 'g', 8))));
        summaryTable_->setItem(row, 6, readOnlyItem(AnalysisEngine::thresholdText(summary.t90)));

        thresholdTable_->setItem(row, 0, readOnlyItem(summary.catalyst));
        thresholdTable_->setItem(row, 1, readOnlyItem(QStringLiteral("%1%").arg(QString::number(summary.retentionPercent, 'f', 1))));
        thresholdTable_->setItem(row, 2, readOnlyItem(AnalysisEngine::thresholdText(summary.t95)));
        thresholdTable_->setItem(row, 3, readOnlyItem(AnalysisEngine::thresholdText(summary.t90)));
        thresholdTable_->setItem(row, 4, readOnlyItem(AnalysisEngine::thresholdText(summary.t80)));
        thresholdTable_->setItem(row, 5, readOnlyItem(QString::number(summary.initialPerformance, 'g', 8)));
        thresholdTable_->setItem(row, 6, readOnlyItem(QString::number(summary.latestPerformance, 'g', 8)));
    }
}

void MainWindow::updateProjectUi() {
    if (projectPathLabel_) {
        projectPathLabel_->setText(currentProjectPath_.isEmpty()
            ? QStringLiteral("未命名项目")
            : currentProjectPath_);
    }
    if (projectStateLabel_) {
        if (currentProjectPath_.isEmpty()) {
            projectStateLabel_->setText(projectDirty_
                ? QStringLiteral("未保存 · %1 条实验记录").arg(records_.size())
                : QStringLiteral("尚未保存 · 空白项目"));
        } else {
            projectStateLabel_->setText(projectDirty_
                ? QStringLiteral("有未保存更改 · %1 条实验记录").arg(records_.size())
                : QStringLiteral("已保存 · %1 条实验记录").arg(records_.size()));
        }
    }

    QString title = QStringLiteral("Catalyst Longevity Research");
    title += currentProjectPath_.isEmpty()
        ? QStringLiteral(" — 未命名项目")
        : QStringLiteral(" — %1").arg(QFileInfo(currentProjectPath_).completeBaseName());
    if (projectDirty_) title += QStringLiteral(" *");
    setWindowTitle(title);
}

void MainWindow::setStatus(const QString& text, bool error) {
    statusLabel_->setText(text);
    statusLabel_->setStyleSheet(error
        ? QStringLiteral("color: #B91C1C; padding: 2px 8px;")
        : QStringLiteral("color: #4B5563; padding: 2px 8px;"));
}

} // namespace catalyst
