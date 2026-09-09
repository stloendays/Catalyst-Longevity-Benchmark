#include "mainwindow.h"

#include "analysisengine.h"
#include "chartwidget.h"
#include "conditionguard.h"
#include "csvreader.h"
#include "evidencepage.h"
#include "projectstore.h"
#include "reportexporter.h"
#include "researchadvisor.h"

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
#include <QSet>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QStyle>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>

namespace catalyst {

namespace {

QString gptMonochromeStyle() {
    return QStringLiteral(R"(
        QMainWindow, QWidget { background:#F7F7F8; color:#111111; font-family:"Microsoft YaHei UI"; font-size:13px; }
        QMainWindow { background:#F7F7F8; }
        QStatusBar { background:#FFFFFF; color:#71717A; border-top:1px solid #E7E7E9; min-height:30px; }
        QStatusBar QLabel { color:#71717A; padding:0 8px; }
        #sidebar { background:#111111; border-right:1px solid #242424; }
        #brand { color:#FFFFFF; background:transparent; border:none; padding:8px 8px 14px; font-size:16px; font-weight:700; letter-spacing:.3px; }
        #sidebarFoot { color:#737373; font-size:11px; padding:8px 4px; }
        #navButton { color:#D4D4D4; background:transparent; border:1px solid transparent; border-radius:10px; padding:10px 12px; text-align:left; font-weight:500; min-height:24px; }
        #navButton:hover { background:#242424; border-color:#333333; color:#FFFFFF; }
        #navButton:checked { background:#2F2F2F; border-color:#3C3C3C; color:#FFFFFF; font-weight:650; }
        #navButton:pressed { background:#383838; }
        #pageHeading { color:#111111; font-size:24px; font-weight:700; }
        #mutedText { color:#71717A; line-height:1.55; }
        #sectionTitle { color:#18181B; font-size:15px; font-weight:700; }
        #metricTitle { color:#71717A; font-size:12px; font-weight:500; }
        #metricValue { color:#111111; font-size:21px; font-weight:700; }
        #sourcePath { color:#27272A; font-weight:650; }
        #metricCard { background:#FFFFFF; border:1px solid #E7E7E9; border-radius:14px; min-height:70px; }
        #metricCard:hover { background:#FCFCFC; border-color:#CFCFD2; }
        #decisionCard { background:#FFFFFF; border:1px solid #E4E4E7; border-radius:14px; min-height:105px; }
        #decisionCard:hover { background:#FCFCFC; border-color:#B8B8BE; }
        #decisionTitle { color:#52525B; font-size:12px; font-weight:650; }
        #decisionDetail { color:#71717A; font-size:12px; }
        #panel, #gptSurface, #evidenceSurface, #aiSurface { background:#FFFFFF; border:1px solid #E7E7E9; border-radius:14px; }
        #panel:hover, #gptSurface:hover, #evidenceSurface:hover, #aiSurface:hover { background:#FEFEFE; border-color:#CFCFD2; }
        #gptSurface { border-color:#DEDEE1; }
        #evidenceSurface { border-left:3px solid #18181B; }
        #aiSurface { background:#FAFAFA; border-color:#DCDCE0; }
        #infoPanel { background:#FAFAFA; border:1px solid #E4E4E7; border-radius:12px; }
        #statusNeutral, #statusGood, #statusWarn, #statusBad { border-radius:9px; padding:5px 9px; font-size:12px; font-weight:650; }
        #statusNeutral { color:#52525B; background:#F4F4F5; border:1px solid #E4E4E7; }
        #statusGood { color:#166534; background:#F0FDF4; border:1px solid #BBF7D0; }
        #statusWarn { color:#92400E; background:#FFFBEB; border:1px solid #FDE68A; }
        #statusBad { color:#991B1B; background:#FEF2F2; border:1px solid #FECACA; }
        #guardStatus { color:#18181B; font-size:14px; font-weight:700; padding:4px 0; }
        #primaryButton { background:#111111; color:#FFFFFF; border:1px solid #111111; border-radius:10px; padding:9px 16px; font-weight:600; min-height:22px; }
        #primaryButton:hover { background:#2F2F2F; border-color:#2F2F2F; }
        #primaryButton:pressed { background:#444444; border-color:#444444; }
        #primaryButton:disabled { background:#B4B4B4; border-color:#B4B4B4; color:#F5F5F5; }
        #secondaryButton { background:#FFFFFF; color:#27272A; border:1px solid #D4D4D8; border-radius:10px; padding:9px 16px; font-weight:600; min-height:22px; }
        #secondaryButton:hover { background:#F4F4F5; border-color:#A1A1AA; color:#111111; }
        #secondaryButton:pressed { background:#EDEDEF; }
        QLineEdit, QComboBox, QDoubleSpinBox, QTextEdit { background:#FFFFFF; color:#18181B; border:1px solid #D4D4D8; border-radius:9px; padding:7px 9px; selection-background-color:#27272A; selection-color:#FFFFFF; }
        QLineEdit:hover, QComboBox:hover, QDoubleSpinBox:hover, QTextEdit:hover { border-color:#A1A1AA; }
        QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QTextEdit:focus { border:1px solid #52525B; background:#FFFFFF; }
        QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QTextEdit:disabled { background:#F4F4F5; color:#A1A1AA; }
        QComboBox::drop-down, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { border:none; background:transparent; }
        QTableWidget { background:#FFFFFF; alternate-background-color:#FAFAFA; border:1px solid #E4E4E7; border-radius:10px; gridline-color:#F0F0F1; selection-background-color:#F0F0F1; selection-color:#111111; }
        QHeaderView::section { background:#F7F7F8; color:#52525B; border:none; border-right:1px solid #ECECEF; border-bottom:1px solid #E4E4E7; padding:9px 8px; font-weight:650; }
        QTableWidget::item { padding:7px; border:none; }
        QTableWidget::item:hover { background:#F5F5F6; }
        QTableWidget::item:selected { background:#EDEDEF; color:#111111; }
        QTabWidget::pane { background:#FFFFFF; border:1px solid #E4E4E7; border-radius:12px; top:-1px; }
        QTabBar::tab { background:#F4F4F5; color:#71717A; border:1px solid #E4E4E7; padding:8px 18px; margin-right:4px; border-top-left-radius:8px; border-top-right-radius:8px; }
        QTabBar::tab:hover { background:#ECECEE; color:#27272A; }
        QTabBar::tab:selected { background:#FFFFFF; color:#111111; border-bottom-color:#FFFFFF; font-weight:650; }
        QScrollBar:vertical { background:transparent; width:10px; margin:3px 2px; }
        QScrollBar::handle:vertical { background:#D4D4D8; border-radius:4px; min-height:28px; }
        QScrollBar::handle:vertical:hover { background:#A1A1AA; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar:horizontal { background:transparent; height:10px; margin:2px 3px; }
        QScrollBar::handle:horizontal { background:#D4D4D8; border-radius:4px; min-width:28px; }
        QScrollBar::handle:horizontal:hover { background:#A1A1AA; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
        QToolTip { background:#18181B; color:#FFFFFF; border:1px solid #3F3F46; border-radius:7px; padding:6px 8px; }
    )");
}

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

QFrame* decisionCard(const QString& title, QLabel** statusLabel, QLabel** detailLabel) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("decisionCard"));
    card->setAttribute(Qt::WA_Hover, true);
    card->setMouseTracking(true);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(7);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("decisionTitle"));
    auto* state = new QLabel(QStringLiteral("暂无数据"));
    state->setObjectName(QStringLiteral("statusNeutral"));
    auto* detail = new QLabel(QStringLiteral("暂无结果。"));
    detail->setWordWrap(true);
    detail->setObjectName(QStringLiteral("decisionDetail"));

    layout->addWidget(titleLabel);
    layout->addWidget(state, 0, Qt::AlignLeft);
    layout->addWidget(detail);
    layout->addStretch();
    *statusLabel = state;
    *detailLabel = detail;
    return card;
}

void setStatusChip(QLabel* label, const QString& text, const QString& objectName) {
    if (!label) return;
    label->setText(text);
    if (label->objectName() != objectName) {
        label->setObjectName(objectName);
        if (label->style()) {
            label->style()->unpolish(label);
            label->style()->polish(label);
        }
    }
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
    for (const auto& field : fields) labels.append(conditionFieldLabel(field));
    return labels.join(QStringLiteral("、"));
}

void configureTable(QTableWidget* table) {
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
}

QStringList catalystNamesFromRecords(const QVector<Record>& records) {
    QSet<QString> names;
    for (const auto& record : records) {
        const QString name = record.catalyst.trimmed();
        if (!name.isEmpty()) names.insert(name);
    }
    QStringList result(names.cbegin(), names.cend());
    result.sort(Qt::CaseInsensitive);
    return result;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    resize(1380, 860);
    setMinimumSize(1120, 720);
    buildUi();
    applyTheme();
    newProject();
    QTimer::singleShot(0, this, [this]() {
        setStyleSheet(gptMonochromeStyle());
        for (auto* frame : findChildren<QFrame*>()) {
            frame->setAttribute(Qt::WA_Hover, true);
            frame->setMouseTracking(true);
            if (frame->objectName() != QStringLiteral("sidebar")) frame->setGraphicsEffect(nullptr);
        }
        for (auto* table : findChildren<QTableWidget*>()) table->setMouseTracking(true);
    });
}

void MainWindow::buildUi() {
    auto* central = new QWidget;
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(buildSidebar());

    pages_ = new QStackedWidget;
    pages_->addWidget(buildProjectPage());
    pages_->addWidget(buildOverviewPage());
    pages_->addWidget(buildDataPage());
    evidencePage_ = new EvidencePage;
    pages_->addWidget(evidencePage_);
    pages_->addWidget(buildAnalysisPage());
    pages_->addWidget(buildAiPage());
    pages_->addWidget(buildSettingsPage());
    root->addWidget(pages_, 1);

    connect(evidencePage_, &EvidencePage::evidenceChanged, this, [this]() {
        projectDirty_ = true;
        updateProjectUi();
        refreshDecisionOverview();
        setStatus(QStringLiteral("证据候选已更新；保存项目可持久化当前绑定与复核状态。"));
    });

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
        QStringLiteral("数据"),
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
    layout->addWidget(heading(QStringLiteral("项目")));
    layout->addWidget(muted(QStringLiteral(
        "项目文件使用本地 SQLite 保存实验记录、实验条件和资料证据候选。关闭软件后可以直接重新打开 .clrproj 继续分析。")));

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
        ".clrproj 内部为 SQLite 数据库。证据候选的来源、摘要、绑定催化剂、绑定时间和人工复核状态会随项目一起保存，但不会自动改写实验观测数据。")));
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
    titleBox->addWidget(heading(QStringLiteral("首页")));
    titleBox->addWidget(muted(QStringLiteral(
        "查看催化剂长期表现、寿命指标和实验条件检查结果。")));
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

    auto* decisionTop = new QHBoxLayout;
    auto* decisionHeading = new QLabel(QStringLiteral("分析概况"));
    decisionHeading->setObjectName(QStringLiteral("sectionTitle"));
    decisionTop->addWidget(decisionHeading);
    decisionTop->addStretch();
    decisionTop->addWidget(muted(QStringLiteral("先检查数据和实验条件，再查看比较结果。状态颜色只表示当前资料是否完整。")));
    layout->addLayout(decisionTop);

    auto* decisions = new QGridLayout;
    decisions->setHorizontalSpacing(12);
    decisions->setVerticalSpacing(12);
    decisions->addWidget(decisionCard(QStringLiteral("条件检查"), &decisionComparability_, &decisionComparabilityDetail_), 0, 0);
    decisions->addWidget(decisionCard(QStringLiteral("当前领先"), &decisionLeader_, &decisionLeaderDetail_), 0, 1);
    decisions->addWidget(decisionCard(QStringLiteral("T90 状态"), &decisionT90_, &decisionT90Detail_), 0, 2);
    decisions->addWidget(decisionCard(QStringLiteral("资料状态"), &decisionEvidence_, &decisionEvidenceDetail_), 0, 3);
    layout->addLayout(decisions);

    auto* metrics = new QGridLayout;
    metrics->setHorizontalSpacing(12);
    metrics->addWidget(metricCard(QStringLiteral("催化剂"), &metricCatalysts_), 0, 0);
    metrics->addWidget(metricCard(QStringLiteral("数据点"), &metricPoints_), 0, 1);
    metrics->addWidget(metricCard(QStringLiteral("最长测试"), &metricLongest_), 0, 2);
    metrics->addWidget(metricCard(QStringLiteral("条件守门"), &metricCondition_), 0, 3);
    metrics->addWidget(metricCard(QStringLiteral("共同时间领先"), &metricLeader_), 0, 4);
    layout->addLayout(metrics);

    auto* chartFrame = new QFrame;
    chartFrame->setObjectName(QStringLiteral("gptSurface"));
    auto* chartLayout = new QVBoxLayout(chartFrame);
    chartLayout->setContentsMargins(18, 16, 18, 16);
    auto* chartHead = new QHBoxLayout;
    auto* chartTitle = new QLabel(QStringLiteral("长期性能曲线"));
    chartTitle->setObjectName(QStringLiteral("sectionTitle"));
    auto* chartHint = muted(QStringLiteral("悬停查看数据 · 单击曲线可聚焦，再次单击取消"));
    chartHead->addWidget(chartTitle);
    chartHead->addStretch();
    chartHead->addWidget(chartHint);
    chart_ = new ChartWidget;
    chartLayout->addLayout(chartHead);
    chartLayout->addWidget(chart_, 1);
    layout->addWidget(chartFrame, 1);

    auto* tableFrame = new QFrame;
    tableFrame->setObjectName(QStringLiteral("gptSurface"));
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
    titleBox->addWidget(heading(QStringLiteral("数据")));
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

    auto* checkFrame = new QFrame;
    checkFrame->setObjectName(QStringLiteral("gptSurface"));
    auto* checkLayout = new QVBoxLayout(checkFrame);
    checkLayout->setContentsMargins(18, 14, 18, 14);
    checkLayout->setSpacing(10);
    auto* checkTop = new QHBoxLayout;
    auto* checkTitle = new QLabel(QStringLiteral("数据检查"));
    checkTitle->setObjectName(QStringLiteral("sectionTitle"));
    dataCheckStatus_ = new QLabel(QStringLiteral("暂无数据"));
    dataCheckStatus_->setObjectName(QStringLiteral("statusNeutral"));
    dataCheckScore_ = muted(QStringLiteral("完整度评分：—"));
    checkTop->addWidget(checkTitle);
    checkTop->addWidget(dataCheckStatus_);
    checkTop->addStretch();
    checkTop->addWidget(dataCheckScore_);
    checkLayout->addLayout(checkTop);
    checkLayout->addWidget(muted(QStringLiteral("自动检查重复时间点、异常数值、实验条件缺失和不同催化剂之间的条件差异。")));
    dataCheckTable_ = new QTableWidget(0, 4);
    dataCheckTable_->setHorizontalHeaderLabels({
        QStringLiteral("状态"), QStringLiteral("范围"), QStringLiteral("发现"), QStringLiteral("建议")});
    configureTable(dataCheckTable_);
    dataCheckTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    dataCheckTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    dataCheckTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    dataCheckTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    dataCheckTable_->setMaximumHeight(210);
    checkLayout->addWidget(dataCheckTable_);
    layout->addWidget(checkFrame);

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
    layout->addWidget(heading(QStringLiteral("分析")));
    layout->addWidget(muted(QStringLiteral(
        "查看寿命区间、同时间对比和下一步实验建议。比较前会自动检查实验条件。")));

    auto* guardFrame = new QFrame;
    guardFrame->setObjectName(QStringLiteral("panel"));
    auto* guardLayout = new QVBoxLayout(guardFrame);
    guardLayout->setContentsMargins(18, 16, 18, 16);
    auto* guardTitle = new QLabel(QStringLiteral("条件检查"));
    guardTitle->setObjectName(QStringLiteral("sectionTitle"));
    conditionStatusLabel_ = new QLabel(QStringLiteral("—"));
    conditionStatusLabel_->setObjectName(QStringLiteral("statusNeutral"));
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

    auto* analysisTabs = new QTabWidget;

    auto* lifetimeTab = new QWidget;
    auto* lifetimeLayout = new QVBoxLayout(lifetimeTab);
    lifetimeLayout->setContentsMargins(12, 12, 12, 12);
    thresholdTable_ = new QTableWidget(0, 7);
    thresholdTable_->setHorizontalHeaderLabels({
        QStringLiteral("催化剂"), QStringLiteral("保持率"), QStringLiteral("T95"),
        QStringLiteral("T90"), QStringLiteral("T80"), QStringLiteral("初始值"), QStringLiteral("最新值")});
    configureTable(thresholdTable_);
    lifetimeLayout->addWidget(thresholdTable_);
    analysisTabs->addTab(lifetimeTab, QStringLiteral("寿命指标"));

    auto* compareTab = new QWidget;
    auto* compareLayout = new QVBoxLayout(compareTab);
    compareLayout->setContentsMargins(12, 12, 12, 12);
    compareLayout->addWidget(muted(QStringLiteral("按两个催化剂最近的共同观测时间进行比较；实验条件不一致时不会给出直接领先结论。")));
    comparisonTable_ = new QTableWidget(0, 8);
    comparisonTable_->setHorizontalHeaderLabels({
        QStringLiteral("催化剂 A"), QStringLiteral("催化剂 B"), QStringLiteral("共同时间(h)"),
        QStringLiteral("A 性能"), QStringLiteral("B 性能"), QStringLiteral("差值"),
        QStringLiteral("状态"), QStringLiteral("结果")});
    configureTable(comparisonTable_);
    compareLayout->addWidget(comparisonTable_);
    analysisTabs->addTab(compareTab, QStringLiteral("同时间对比"));

    auto* adviceTab = new QWidget;
    auto* adviceLayout = new QVBoxLayout(adviceTab);
    adviceLayout->setContentsMargins(12, 12, 12, 12);
    adviceLayout->addWidget(muted(QStringLiteral("根据当前观测点、T90 区间和实验条件自动生成下一轮实验建议。建议为规则计算结果，可直接用于实验计划讨论。")));
    adviceTable_ = new QTableWidget(0, 5);
    adviceTable_->setHorizontalHeaderLabels({
        QStringLiteral("优先级"), QStringLiteral("催化剂"), QStringLiteral("建议"),
        QStringLiteral("原因"), QStringLiteral("下一步")});
    configureTable(adviceTable_);
    adviceLayout->addWidget(adviceTable_);
    analysisTabs->addTab(adviceTab, QStringLiteral("实验建议"));

    layout->addWidget(analysisTabs, 1);

    auto* note = new QFrame;
    note->setObjectName(QStringLiteral("infoPanel"));
    auto* noteLayout = new QVBoxLayout(note);
    noteLayout->addWidget(new QLabel(QStringLiteral("结果说明")));
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
    layout->addWidget(heading(QStringLiteral("AI 助手")));
    layout->addWidget(muted(QStringLiteral(
        "AI 只使用你已经确认的资料进行分析，不会修改原始实验数据。")));

    auto* readiness = new QFrame;
    readiness->setObjectName(QStringLiteral("aiSurface"));
    auto* readinessLayout = new QHBoxLayout(readiness);
    readinessLayout->setContentsMargins(20, 16, 20, 16);
    readinessLayout->setSpacing(14);
    auto* readinessTitle = new QLabel(QStringLiteral("资料准备"));
    readinessTitle->setObjectName(QStringLiteral("sectionTitle"));
    aiEvidenceDetail_ = muted(QStringLiteral("确认后的资料可以提供给 AI，未确认内容不会自动使用。"));
    aiEvidenceStatus_ = new QLabel(QStringLiteral("暂无资料"));
    aiEvidenceStatus_->setObjectName(QStringLiteral("statusNeutral"));
    readinessLayout->addWidget(readinessTitle);
    readinessLayout->addWidget(aiEvidenceDetail_, 1);
    readinessLayout->addWidget(aiEvidenceStatus_);
    layout->addWidget(readiness);

    auto* stages = new QGridLayout;
    stages->setHorizontalSpacing(12);
    stages->setVerticalSpacing(12);
    const QStringList titles = {
        QStringLiteral("01  已确认资料"), QStringLiteral("02  智能分析"),
        QStringLiteral("03  结果核对"), QStringLiteral("04  分析记录")
    };
    const QStringList descriptions = {
        QStringLiteral("只使用已关联催化剂、时间并由你确认的资料。"),
        QStringLiteral("结合实验数据和已确认资料生成分析与建议。"),
        QStringLiteral("检查资料来源、实验条件和结论是否对应。"),
        QStringLiteral("保存本次使用的资料、确认状态和分析结果。")
    };
    const QStringList states = {
        QStringLiteral("可使用"), QStringLiteral("未配置"),
        QStringLiteral("未配置"), QStringLiteral("已开启")
    };
    for (int i = 0; i < titles.size(); ++i) {
        auto* card = new QFrame;
        card->setObjectName(QStringLiteral("aiSurface"));
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 18, 20, 18);
        cardLayout->setSpacing(8);
        auto* title = new QLabel(titles[i]);
        title->setObjectName(QStringLiteral("sectionTitle"));
        auto* state = new QLabel(states[i]);
        state->setObjectName(i == 0 || i == 3 ? QStringLiteral("statusNeutral") : QStringLiteral("statusWarn"));
        if (i == 0) aiPacketStageStatus_ = state;
        cardLayout->addWidget(title);
        cardLayout->addWidget(muted(descriptions[i]));
        cardLayout->addStretch();
        cardLayout->addWidget(state, 0, Qt::AlignLeft);
        stages->addWidget(card, i / 2, i % 2);
    }
    layout->addLayout(stages, 1);

    auto* boundary = new QFrame;
    boundary->setObjectName(QStringLiteral("infoPanel"));
    auto* boundaryLayout = new QVBoxLayout(boundary);
    boundaryLayout->setContentsMargins(18, 14, 18, 14);
    auto* boundaryTitle = new QLabel(QStringLiteral("AI 使用范围"));
    boundaryTitle->setObjectName(QStringLiteral("sectionTitle"));
    boundaryLayout->addWidget(boundaryTitle);
    boundaryLayout->addWidget(muted(QStringLiteral(
        "未关联或未确认的资料不会提供给 AI。分析结果会保留资料来源和确认状态，方便后续核对。")));
    layout->addWidget(boundary);
    return page;
}

QWidget* MainWindow::buildSettingsPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 28, 34, 28);
    layout->setSpacing(16);
    layout->addWidget(heading(QStringLiteral("设置")));

    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("panel"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->addWidget(new QLabel(QStringLiteral("催化剂寿命分析与实验决策软件 V1.0")));
    cardLayout->addWidget(muted(QStringLiteral("V1.0 · Windows 原生桌面版 · C++20 + Qt 6 + SQLite")));
    cardLayout->addSpacing(10);
    cardLayout->addWidget(new QLabel(QStringLiteral("运行方式：本地桌面窗口，不启动浏览器，不依赖 Streamlit。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("数据输入：CSV / Excel .xlsx。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("项目存储：本地 .clrproj 文件（实验数据、资料关联和确认状态）。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("核心功能：数据检查、寿命分析、同时间对比、实验建议、资料整理和 PDF 报告。")));
    cardLayout->addStretch();
    layout->addWidget(card, 1);
    return page;
}

void MainWindow::applyTheme() {
    setStyleSheet(gptMonochromeStyle());
}

void MainWindow::newProject() {
    if (!confirmProjectTransition()) return;
    currentProjectPath_.clear();
    projectDirty_ = false;
    records_.clear();
    sourceLabelText_ = QStringLiteral("未加载数据");
    analysis_ = AnalysisResult{};
    if (sourceLabel_) sourceLabel_->setText(sourceLabelText_);
    if (evidencePage_) {
        evidencePage_->clearEvidence();
        evidencePage_->setCatalystNames({});
    }
    refreshRawTable();
    refreshAnalysisViews();
    updateProjectUi();
    setStatus(QStringLiteral("已新建空白项目。"));
}

void MainWindow::openProject() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开催化剂寿命分析项目"),
        QString(),
        QStringLiteral("Catalyst Longevity 项目 (*.clrproj);;所有文件 (*.*)"));
    if (path.isEmpty() || !confirmProjectTransition()) return;

    QVector<Record> loaded;
    QVector<EvidenceItem> loadedEvidence;
    QString message;
    if (!ProjectStore::loadProject(path, &loaded, &loadedEvidence, &message)) {
        QMessageBox::warning(this, QStringLiteral("打开项目失败"), message);
        setStatus(message, true);
        return;
    }

    currentProjectPath_ = path;
    if (evidencePage_) evidencePage_->setEvidenceItems(loadedEvidence);
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
        QStringLiteral("保存催化剂寿命分析项目"),
        currentProjectPath_.isEmpty() ? QStringLiteral("催化剂寿命分析项目.clrproj") : currentProjectPath_,
        QStringLiteral("催化剂寿命分析项目 (*.clrproj)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".clrproj");
    saveProjectTo(path);
}

bool MainWindow::saveProjectTo(const QString& path) {
    QString message;
    const QVector<EvidenceItem> evidence = evidencePage_
        ? evidencePage_->evidenceItems()
        : QVector<EvidenceItem>{};
    if (!ProjectStore::saveProject(path, records_, evidence, &message)) {
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
    const QVector<EvidenceItem> evidence = evidencePage_
        ? evidencePage_->evidenceItems()
        : QVector<EvidenceItem>{};
    if (!ReportExporter::exportPdf(path, analysis_, sourceLabelText_, evidence, &message)) {
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
            QStringLiteral("催化剂寿命分析项目.clrproj"),
            QStringLiteral("催化剂寿命分析项目 (*.clrproj)"));
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
    if (evidencePage_) evidencePage_->setCatalystNames(catalystNamesFromRecords(records_));
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
    QString conditionStyle = QStringLiteral("statusNeutral");
    if (analysis_.conditionAudit.status == ConditionAuditStatus::MatchedOnProvidedConditions) {
        conditionStyle = QStringLiteral("statusGood");
    } else if (analysis_.conditionAudit.status == ConditionAuditStatus::ConditionsNotProvided) {
        conditionStyle = QStringLiteral("statusWarn");
    } else if (analysis_.conditionAudit.blocksDirectRanking()) {
        conditionStyle = QStringLiteral("statusBad");
    }
    setStatusChip(conditionStatusLabel_, ConditionGuard::statusText(analysis_.conditionAudit.status), conditionStyle);
}
    if (conditionMessageLabel_) {
        conditionMessageLabel_->setText(analysis_.conditionAudit.message.isEmpty()
            ? QStringLiteral("暂无可用数据。")
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
    refreshDecisionOverview();
    refreshResearchSupportViews();
}

void MainWindow::refreshDecisionOverview() {
    if (!decisionComparability_) return;

    if (analysis_.totalObservations <= 0) {
        setStatusChip(decisionComparability_, QStringLiteral("暂无数据"), QStringLiteral("statusNeutral"));
        decisionComparabilityDetail_->setText(QStringLiteral("导入实验数据后检查温度、空速、压力和进料条件。"));
    } else if (analysis_.conditionAudit.blocksDirectRanking()) {
        setStatusChip(decisionComparability_, QStringLiteral("条件不一致"), QStringLiteral("statusBad"));
        decisionComparabilityDetail_->setText(QStringLiteral("不同催化剂的实验条件不一致，暂不进行直接比较。"));
    } else if (analysis_.conditionAudit.status == ConditionAuditStatus::MatchedOnProvidedConditions) {
        setStatusChip(decisionComparability_, QStringLiteral("条件一致"), QStringLiteral("statusGood"));
        decisionComparabilityDetail_->setText(QStringLiteral("已填写的实验条件一致，可以继续比较。"));
    } else {
        setStatusChip(decisionComparability_, QStringLiteral("信息不完整"), QStringLiteral("statusWarn"));
        decisionComparabilityDetail_->setText(QStringLiteral("暂未发现冲突，但实验条件填写不完整。"));
    }

    if (analysis_.totalObservations <= 0) {
        setStatusChip(decisionLeader_, QStringLiteral("暂无结果"), QStringLiteral("statusNeutral"));
        decisionLeaderDetail_->setText(QStringLiteral("有共同观测时间后才会显示比较结果。"));
    } else if (analysis_.conditionAudit.blocksDirectRanking()) {
        setStatusChip(decisionLeader_, QStringLiteral("暂不比较"), QStringLiteral("statusBad"));
        decisionLeaderDetail_->setText(QStringLiteral("请先处理实验条件差异。"));
    } else if (analysis_.latestSharedTimeHours.has_value() && !analysis_.latestSharedLeader.isEmpty()) {
        setStatusChip(decisionLeader_, analysis_.latestSharedLeader, QStringLiteral("statusGood"));
        decisionLeaderDetail_->setText(QStringLiteral("共同观测时间：%1 h。")
            .arg(QString::number(*analysis_.latestSharedTimeHours, 'g', 8)));
    } else {
        setStatusChip(decisionLeader_, QStringLiteral("无共同时间点"), QStringLiteral("statusWarn"));
        decisionLeaderDetail_->setText(QStringLiteral("各催化剂暂时没有相同的观测时间。"));
    }

    const int totalCatalysts = analysis_.catalysts.size();
    int t90Touched = 0;
    for (const auto& summary : analysis_.catalysts) {
        if (summary.t90.status != ThresholdStatus::RightCensored) ++t90Touched;
    }
    if (totalCatalysts == 0) {
        setStatusChip(decisionT90_, QStringLiteral("暂无数据"), QStringLiteral("statusNeutral"));
        decisionT90Detail_->setText(QStringLiteral("T90 保留左删失、区间删失与右删失语义。"));
    } else if (t90Touched == totalCatalysts) {
        setStatusChip(decisionT90_, QStringLiteral("%1/%2 已达到").arg(t90Touched).arg(totalCatalysts), QStringLiteral("statusGood"));
        decisionT90Detail_->setText(QStringLiteral("所有催化剂都已观测到 T90。"));
    } else if (t90Touched > 0) {
        setStatusChip(decisionT90_, QStringLiteral("%1/%2 已达到").arg(t90Touched).arg(totalCatalysts), QStringLiteral("statusWarn"));
        decisionT90Detail_->setText(QStringLiteral("还有 %1 个在测试结束时仍未达到 T90。").arg(totalCatalysts - t90Touched));
    } else {
        setStatusChip(decisionT90_, QStringLiteral("暂未达到 T90"), QStringLiteral("statusWarn"));
        decisionT90Detail_->setText(QStringLiteral("测试结束时都未达到 T90，目前只能得到寿命下限。"));
    }

    if (!evidencePage_) return;
    const EvidencePacket packet = evidencePage_->evidencePacket();
    if (packet.readyForAi) {
        setStatusChip(decisionEvidence_, QStringLiteral("已确认 %1 条").arg(packet.reviewedContextItems), QStringLiteral("statusGood"));
        decisionEvidenceDetail_->setText(QStringLiteral("另有 %1 条待确认。").arg(packet.pendingItems));
        setStatusChip(aiEvidenceStatus_, QStringLiteral("可用资料 %1 条").arg(packet.reviewedContextItems), QStringLiteral("statusGood"));
        setStatusChip(aiPacketStageStatus_, QStringLiteral("可使用 · %1 条").arg(packet.reviewedContextItems), QStringLiteral("statusGood"));
        if (aiEvidenceDetail_) aiEvidenceDetail_->setText(QStringLiteral("已准备好可供 AI 使用的资料；未确认内容不会被读取。"));
    } else if (packet.pendingItems > 0) {
        setStatusChip(decisionEvidence_, QStringLiteral("待确认 %1 条").arg(packet.pendingItems), QStringLiteral("statusWarn"));
        decisionEvidenceDetail_->setText(QStringLiteral("关联催化剂和时间并确认后，资料才会提供给 AI。"));
        setStatusChip(aiEvidenceStatus_, QStringLiteral("待确认 %1 条").arg(packet.pendingItems), QStringLiteral("statusWarn"));
        setStatusChip(aiPacketStageStatus_, QStringLiteral("暂不可用"), QStringLiteral("statusWarn"));
        if (aiEvidenceDetail_) aiEvidenceDetail_->setText(QStringLiteral("已有资料，但还需要完成关联和确认。"));
    } else {
        setStatusChip(decisionEvidence_, QStringLiteral("暂无资料"), QStringLiteral("statusNeutral"));
        decisionEvidenceDetail_->setText(QStringLiteral("请在“资料”中导入论文，并完成关联和确认。"));
        setStatusChip(aiEvidenceStatus_, QStringLiteral("暂无资料"), QStringLiteral("statusNeutral"));
        setStatusChip(aiPacketStageStatus_, QStringLiteral("暂无资料"), QStringLiteral("statusNeutral"));
        if (aiEvidenceDetail_) aiEvidenceDetail_->setText(QStringLiteral("目前还没有可供 AI 使用的已确认资料。"));
    }
}

void MainWindow::refreshResearchSupportViews() {
    const DataCheckResult check = ResearchAdvisor::checkData(records_, analysis_);
    if (dataCheckStatus_) {
        QString style = QStringLiteral("statusGood");
        if (records_.isEmpty()) style = QStringLiteral("statusNeutral");
        else if (check.errorCount > 0) style = QStringLiteral("statusBad");
        else if (check.warningCount > 0) style = QStringLiteral("statusWarn");
        setStatusChip(dataCheckStatus_, check.statusText(), style);
    }
    if (dataCheckScore_) {
        dataCheckScore_->setText(records_.isEmpty()
            ? QStringLiteral("完整度评分：—")
            : QStringLiteral("完整度评分：%1 / 100 · %2 个需处理 · %3 个建议")
                .arg(check.score).arg(check.errorCount).arg(check.warningCount));
    }
    if (dataCheckTable_) {
        dataCheckTable_->setRowCount(check.items.size());
        for (qsizetype row = 0; row < check.items.size(); ++row) {
            const auto& item = check.items[row];
            auto* level = readOnlyItem(ResearchAdvisor::checkLevelText(item.level));
            if (item.level == CheckLevel::Error) {
                level->setForeground(QColor(QStringLiteral("#991B1B")));
                level->setBackground(QColor(QStringLiteral("#FEF2F2")));
            } else if (item.level == CheckLevel::Warning) {
                level->setForeground(QColor(QStringLiteral("#92400E")));
                level->setBackground(QColor(QStringLiteral("#FFFBEB")));
            } else {
                level->setForeground(QColor(QStringLiteral("#52525B")));
                level->setBackground(QColor(QStringLiteral("#F4F4F5")));
            }
            dataCheckTable_->setItem(row, 0, level);
            dataCheckTable_->setItem(row, 1, readOnlyItem(item.scope));
            dataCheckTable_->setItem(row, 2, readOnlyItem(item.issue));
            dataCheckTable_->setItem(row, 3, readOnlyItem(item.suggestion));
        }
        dataCheckTable_->resizeRowsToContents();
    }

    if (comparisonTable_) {
        const auto comparisons = ResearchAdvisor::pairComparisons(records_, analysis_);
        comparisonTable_->setRowCount(comparisons.size());
        for (qsizetype row = 0; row < comparisons.size(); ++row) {
            const auto& item = comparisons[row];
            comparisonTable_->setItem(row, 0, readOnlyItem(item.catalystA));
            comparisonTable_->setItem(row, 1, readOnlyItem(item.catalystB));
            comparisonTable_->setItem(row, 2, readOnlyItem(item.sharedTimeHours > 0.0
                ? QString::number(item.sharedTimeHours, 'g', 8) : QStringLiteral("—")));
            comparisonTable_->setItem(row, 3, readOnlyItem(item.sharedTimeHours > 0.0
                ? QString::number(item.performanceA, 'g', 8) : QStringLiteral("—")));
            comparisonTable_->setItem(row, 4, readOnlyItem(item.sharedTimeHours > 0.0
                ? QString::number(item.performanceB, 'g', 8) : QStringLiteral("—")));
            comparisonTable_->setItem(row, 5, readOnlyItem(item.sharedTimeHours > 0.0
                ? QString::number(item.absoluteDifference, 'g', 8) : QStringLiteral("—")));
            auto* status = readOnlyItem(item.status);
            if (item.status == QStringLiteral("条件不一致")) {
                status->setForeground(QColor(QStringLiteral("#991B1B")));
                status->setBackground(QColor(QStringLiteral("#FEF2F2")));
            } else if (item.status == QStringLiteral("仅供参考")) {
                status->setForeground(QColor(QStringLiteral("#92400E")));
                status->setBackground(QColor(QStringLiteral("#FFFBEB")));
            } else if (item.status == QStringLiteral("可比较")) {
                status->setForeground(QColor(QStringLiteral("#166534")));
                status->setBackground(QColor(QStringLiteral("#F0FDF4")));
            }
            comparisonTable_->setItem(row, 6, status);
            const QString outcome = item.comparable && !item.leader.isEmpty()
                ? QStringLiteral("%1 当前较高").arg(item.leader)
                : QStringLiteral("暂不判断");
            comparisonTable_->setItem(row, 7, readOnlyItem(outcome));
        }
        comparisonTable_->resizeRowsToContents();
    }

    if (adviceTable_) {
        const auto advice = ResearchAdvisor::experimentAdvice(records_, analysis_);
        adviceTable_->setRowCount(advice.size());
        for (qsizetype row = 0; row < advice.size(); ++row) {
            const auto& item = advice[row];
            auto* priority = readOnlyItem(ResearchAdvisor::advicePriorityText(item.priority));
            if (item.priority == AdvicePriority::High) {
                priority->setForeground(QColor(QStringLiteral("#991B1B")));
                priority->setBackground(QColor(QStringLiteral("#FEF2F2")));
            } else if (item.priority == AdvicePriority::Important) {
                priority->setForeground(QColor(QStringLiteral("#92400E")));
                priority->setBackground(QColor(QStringLiteral("#FFFBEB")));
            } else {
                priority->setForeground(QColor(QStringLiteral("#52525B")));
                priority->setBackground(QColor(QStringLiteral("#F4F4F5")));
            }
            adviceTable_->setItem(row, 0, priority);
            adviceTable_->setItem(row, 1, readOnlyItem(item.catalyst));
            adviceTable_->setItem(row, 2, readOnlyItem(item.action));
            adviceTable_->setItem(row, 3, readOnlyItem(item.reason));
            adviceTable_->setItem(row, 4, readOnlyItem(item.target));
        }
        adviceTable_->resizeRowsToContents();
    }
}

void MainWindow::updateProjectUi() {
    if (projectPathLabel_) {
        projectPathLabel_->setText(currentProjectPath_.isEmpty()
            ? QStringLiteral("未命名项目")
            : currentProjectPath_);
    }
    if (projectStateLabel_) {
        const qsizetype evidenceCount = evidencePage_ ? evidencePage_->evidenceItems().size() : 0;
        const QString counts = QStringLiteral("%1 条实验记录 · %2 条证据候选")
            .arg(records_.size())
            .arg(evidenceCount);
        if (currentProjectPath_.isEmpty()) {
            projectStateLabel_->setText(projectDirty_
                ? QStringLiteral("未保存 · %1").arg(counts)
                : QStringLiteral("尚未保存 · %1").arg(counts));
        } else {
            projectStateLabel_->setText(projectDirty_
                ? QStringLiteral("有未保存更改 · %1").arg(counts)
                : QStringLiteral("已保存 · %1").arg(counts));
        }
    }

    QString title = QStringLiteral("催化剂寿命分析与实验决策软件 V1.0");
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
