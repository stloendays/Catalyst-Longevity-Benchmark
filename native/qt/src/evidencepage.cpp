#include "evidencepage.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QStyle>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>

namespace catalyst {

namespace {

QLabel* headingLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 20, QFont::DemiBold));
    label->setObjectName(QStringLiteral("pageHeading"));
    return label;
}

QLabel* mutedLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setObjectName(QStringLiteral("mutedText"));
    return label;
}

QFrame* metricCard(const QString& title, QLabel** valueLabel) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("metricCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 11, 14, 11);
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

QString joinNumbers(const QVector<double>& values, const QString& suffix = QString()) {
    if (values.isEmpty()) return QStringLiteral("—");
    QStringList text;
    const qsizetype limit = qMin<qsizetype>(values.size(), 20);
    text.reserve(limit);
    for (qsizetype i = 0; i < limit; ++i) {
        text.append(QStringLiteral("%1%2").arg(QString::number(values[i], 'g', 8), suffix));
    }
    if (values.size() > limit) text.append(QStringLiteral("… 共 %1 项").arg(values.size()));
    return text.join(QStringLiteral("，"));
}

QString categoryLabel(const QString& category) {
    if (category == QStringLiteral("doi")) return QStringLiteral("DOI");
    if (category == QStringLiteral("temperature_c")) return QStringLiteral("温度");
    if (category == QStringLiteral("duration_h")) return QStringLiteral("测试时长");
    if (category == QStringLiteral("ch4_conversion_percent_candidate")) return QStringLiteral("CH4 转化率候选");
    if (category == QStringLiteral("keyword_evidence")) return QStringLiteral("失活/稳定证据词");
    return category;
}

QString statusLabel(const QString& status) {
    if (status == QStringLiteral("candidate_requires_condition_binding")) {
        return QStringLiteral("候选：待绑定");
    }
    if (status == QStringLiteral("bound_to_catalyst_requires_time_condition_review")) {
        return QStringLiteral("已绑定催化剂：待时间/条件复核");
    }
    if (status == QStringLiteral("bound_to_catalyst_time_requires_condition_review")) {
        return QStringLiteral("已绑定催化剂+时间：待条件复核");
    }
    if (status == QStringLiteral("condition_reviewed_context_only")) {
        return QStringLiteral("条件已人工复核：仅作上下文");
    }
    return status;
}

QString displayCandidate(const EvidenceItem& item) {
    if (item.category == QStringLiteral("temperature_c")) return item.valueText + QStringLiteral(" °C");
    if (item.category == QStringLiteral("duration_h")) return item.valueText + QStringLiteral(" h");
    if (item.category == QStringLiteral("ch4_conversion_percent_candidate")) return item.valueText + QStringLiteral("%");
    return item.valueText;
}

bool sameCandidate(const EvidenceItem& a, const EvidenceItem& b) {
    return a.sourceSha256 == b.sourceSha256
        && a.sourcePage == b.sourcePage
        && a.category == b.category
        && a.term == b.term
        && a.valueText == b.valueText
        && a.snippet == b.snippet;
}

QString formatLabel(const QString& format) {
    if (format == QStringLiteral("pdf")) return QStringLiteral("PDF");
    if (format == QStringLiteral("md")) return QStringLiteral("Markdown");
    if (format == QStringLiteral("txt")) return QStringLiteral("TXT");
    if (format == QStringLiteral("csv")) return QStringLiteral("CSV");
    if (format == QStringLiteral("tsv")) return QStringLiteral("TSV");
    return format.toUpper();
}

} // namespace

EvidencePage::EvidencePage(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(34, 28, 34, 28);
    layout->setSpacing(14);

    auto* top = new QHBoxLayout;
    auto* titleBox = new QVBoxLayout;
    titleBox->addWidget(headingLabel(QStringLiteral("资料分析与 Evidence Packet")));
    titleBox->addWidget(mutedLabel(QStringLiteral(
        "原生读取带文本层的 PDF 论文，以及 TXT / Markdown / CSV / TSV。保守提取 DOI、温度、测试时长、CH4 转化率候选值与失活证据，并保留 PDF 页码。只有完成催化剂、时间和人工条件复核的条目才进入 Evidence Packet；候选值不会自动改写实验数据或排名。")));
    top->addLayout(titleBox, 1);

    auto* chooseButton = new QPushButton(QStringLiteral("选择 PDF / 资料文件"));
    chooseButton->setObjectName(QStringLiteral("primaryButton"));
    connect(chooseButton, &QPushButton::clicked, this, &EvidencePage::chooseDocument);
    top->addWidget(chooseButton);
    layout->addLayout(top);

    auto* sourceFrame = new QFrame;
    sourceFrame->setObjectName(QStringLiteral("evidenceSurface"));
    auto* sourceLayout = new QVBoxLayout(sourceFrame);
    sourceLayout->setContentsMargins(18, 14, 18, 14);
    auto* sourceTitle = new QLabel(QStringLiteral("当前资料"));
    sourceTitle->setObjectName(QStringLiteral("sectionTitle"));
    sourceLabel_ = new QLabel(QStringLiteral("尚未加载资料"));
    sourceLabel_->setObjectName(QStringLiteral("sourcePath"));
    sourceLabel_->setWordWrap(true);
    sourceLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    documentInfoLabel_ = mutedLabel(QStringLiteral("—"));
    warningLabel_ = mutedLabel(QString());
    warningLabel_->setStyleSheet(QStringLiteral("color:#92400E;"));
    sourceLayout->addWidget(sourceTitle);
    sourceLayout->addWidget(sourceLabel_);
    sourceLayout->addWidget(documentInfoLabel_);
    sourceLayout->addWidget(warningLabel_);
    layout->addWidget(sourceFrame);

    auto* metrics = new QGridLayout;
    metrics->setHorizontalSpacing(10);
    metrics->addWidget(metricCard(QStringLiteral("页数"), &pageCountLabel_), 0, 0);
    metrics->addWidget(metricCard(QStringLiteral("字符数"), &characterLabel_), 0, 1);
    metrics->addWidget(metricCard(QStringLiteral("DOI"), &doiCountLabel_), 0, 2);
    metrics->addWidget(metricCard(QStringLiteral("温度候选"), &temperatureCountLabel_), 0, 3);
    metrics->addWidget(metricCard(QStringLiteral("时长候选"), &durationCountLabel_), 0, 4);
    metrics->addWidget(metricCard(QStringLiteral("项目证据"), &evidenceCountLabel_), 0, 5);
    layout->addLayout(metrics);

    auto* signalFrame = new QFrame;
    signalFrame->setObjectName(QStringLiteral("gptSurface"));
    auto* signalLayout = new QVBoxLayout(signalFrame);
    signalLayout->setContentsMargins(18, 14, 18, 14);
    auto* signalTitle = new QLabel(QStringLiteral("当前资料提取摘要"));
    signalTitle->setObjectName(QStringLiteral("sectionTitle"));
    signalLayout->addWidget(signalTitle);
    signalsTable_ = new QTableWidget(0, 2);
    signalsTable_->setHorizontalHeaderLabels({QStringLiteral("类别"), QStringLiteral("候选信息")});
    signalsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    signalsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    signalsTable_->verticalHeader()->setVisible(false);
    signalsTable_->setAlternatingRowColors(true);
    signalsTable_->setMaximumHeight(165);
    signalsTable_->setMouseTracking(true);
    signalLayout->addWidget(signalsTable_);
    layout->addWidget(signalFrame);

    auto* evidenceFrame = new QFrame;
    evidenceFrame->setObjectName(QStringLiteral("evidenceSurface"));
    auto* evidenceLayout = new QVBoxLayout(evidenceFrame);
    evidenceLayout->setContentsMargins(18, 14, 18, 14);
    auto* evidenceTitle = new QLabel(QStringLiteral("项目证据候选、绑定与复核"));
    evidenceTitle->setObjectName(QStringLiteral("sectionTitle"));
    evidenceLayout->addWidget(evidenceTitle);

    evidenceTable_ = new QTableWidget(0, 8);
    evidenceTable_->setHorizontalHeaderLabels({
        QStringLiteral("来源"), QStringLiteral("页"), QStringLiteral("类别"), QStringLiteral("候选"),
        QStringLiteral("状态"), QStringLiteral("催化剂"), QStringLiteral("时间(h)"),
        QStringLiteral("原文片段 / 备注")});
    evidenceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    evidenceTable_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    evidenceTable_->verticalHeader()->setVisible(false);
    evidenceTable_->setAlternatingRowColors(true);
    evidenceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    evidenceTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    evidenceTable_->setWordWrap(true);
    evidenceTable_->setMouseTracking(true);
    evidenceLayout->addWidget(evidenceTable_, 1);

    auto* bindRow = new QHBoxLayout;
    catalystCombo_ = new QComboBox;
    catalystCombo_->addItem(QStringLiteral("未绑定催化剂"), QString());
    catalystCombo_->setMinimumWidth(175);
    timeSpin_ = new QDoubleSpinBox;
    timeSpin_->setRange(-1.0, 1000000000.0);
    timeSpin_->setDecimals(3);
    timeSpin_->setValue(-1.0);
    timeSpin_->setSpecialValueText(QStringLiteral("未绑定时间"));
    timeSpin_->setSuffix(QStringLiteral(" h"));
    noteEdit_ = new QLineEdit;
    noteEdit_->setPlaceholderText(QStringLiteral("复核/绑定备注；条件复核时必填"));

    auto* bindButton = new QPushButton(QStringLiteral("绑定"));
    bindButton->setObjectName(QStringLiteral("primaryButton"));
    auto* unbindButton = new QPushButton(QStringLiteral("解除"));
    unbindButton->setObjectName(QStringLiteral("secondaryButton"));
    auto* reviewButton = new QPushButton(QStringLiteral("条件已复核（仅上下文）"));
    reviewButton->setObjectName(QStringLiteral("primaryButton"));
    auto* returnButton = new QPushButton(QStringLiteral("退回待复核"));
    returnButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(bindButton, &QPushButton::clicked, this, &EvidencePage::bindSelectedEvidence);
    connect(unbindButton, &QPushButton::clicked, this, &EvidencePage::unbindSelectedEvidence);
    connect(reviewButton, &QPushButton::clicked, this, &EvidencePage::markConditionReviewed);
    connect(returnButton, &QPushButton::clicked, this, &EvidencePage::returnSelectedToReview);

    bindRow->addWidget(new QLabel(QStringLiteral("催化剂")));
    bindRow->addWidget(catalystCombo_);
    bindRow->addWidget(new QLabel(QStringLiteral("时间")));
    bindRow->addWidget(timeSpin_);
    bindRow->addWidget(noteEdit_, 1);
    bindRow->addWidget(bindButton);
    bindRow->addWidget(reviewButton);
    bindRow->addWidget(returnButton);
    bindRow->addWidget(unbindButton);
    evidenceLayout->addLayout(bindRow);
    layout->addWidget(evidenceFrame, 1);

    auto* packetFrame = new QFrame;
    packetFrame->setObjectName(QStringLiteral("aiSurface"));
    auto* packetLayout = new QVBoxLayout(packetFrame);
    packetLayout->setContentsMargins(18, 14, 18, 14);
    auto* packetTop = new QHBoxLayout;
    auto* packetTitle = new QLabel(QStringLiteral("Evidence Packet · AI 输入边界"));
    packetTitle->setObjectName(QStringLiteral("sectionTitle"));
    packetStatusLabel_ = new QLabel(QStringLiteral("尚无可进入 AI 的证据"));
    packetStatusLabel_->setObjectName(QStringLiteral("statusWarn"));
    packetTop->addWidget(packetTitle);
    packetTop->addStretch();
    packetTop->addWidget(packetStatusLabel_);
    packetLayout->addLayout(packetTop);
    packetLayout->addWidget(mutedLabel(QStringLiteral(
        "Packet 只包含已完成人工条件复核且绑定到明确催化剂与时间的证据；其他候选自动排除。这里展示的 Markdown 将作为后续 AI Analyst / Evidence Critic 的可审计上下文基础。")));
    packetPreview_ = new QTextEdit;
    packetPreview_->setReadOnly(true);
    packetPreview_->setMaximumHeight(210);
    packetPreview_->setPlaceholderText(QStringLiteral("完成至少一条证据的条件复核后，将在此生成 Evidence Packet。"));
    packetLayout->addWidget(packetPreview_);
    layout->addWidget(packetFrame);

    auto* note = new QFrame;
    note->setObjectName(QStringLiteral("infoPanel"));
    auto* noteLayout = new QVBoxLayout(note);
    noteLayout->addWidget(new QLabel(QStringLiteral("PDF 与证据门槛")));
    noteLayout->addWidget(mutedLabel(QStringLiteral(
        "原生 PDF 读取只使用 PDF 自带文本层，不自动 OCR 扫描页；这避免 OCR 错误直接进入实验事实链。“条件已复核”仍只是人工上下文核对状态，不是实验真值认证，也不会自动进入性能轨迹、T90 或直接排名。")));
    layout->addWidget(note);

    resetCurrentDocumentSummary();
    refreshEvidenceTable();
}

QVector<EvidenceItem> EvidencePage::evidenceItems() const {
    return evidenceItems_;
}

EvidencePacket EvidencePage::evidencePacket() const {
    return EvidencePacketBuilder::build(evidenceItems_);
}

void EvidencePage::setEvidenceItems(const QVector<EvidenceItem>& items) {
    evidenceItems_ = items;
    currentDocument_ = DocumentReadResult{};
    documentSignals_ = DocumentSignals{};
    resetCurrentDocumentSummary();
    if (!evidenceItems_.isEmpty()) {
        sourceLabel_->setText(QStringLiteral("已从项目恢复 %1 条证据候选").arg(evidenceItems_.size()));
    }
    refreshEvidenceTable();
}

void EvidencePage::clearEvidence() {
    setEvidenceItems({});
}

void EvidencePage::setCatalystNames(const QStringList& catalystNames) {
    const QString previous = catalystCombo_->currentData().toString();
    catalystCombo_->clear();
    catalystCombo_->addItem(QStringLiteral("未绑定催化剂"), QString());
    QStringList names = catalystNames;
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    for (const QString& name : names) catalystCombo_->addItem(name, name);

    const int previousIndex = catalystCombo_->findData(previous);
    if (previousIndex >= 0) catalystCombo_->setCurrentIndex(previousIndex);
}

void EvidencePage::chooseDocument() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择论文或资料文件"),
        QString(),
        QStringLiteral("研究资料 (*.pdf *.txt *.md *.csv *.tsv);;PDF 论文 (*.pdf);;文本资料 (*.txt *.md *.csv *.tsv);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    DocumentReadResult readResult;
    QString message;
    if (!DocumentAnalyzer::readDocument(path, &readResult, &message)) {
        QMessageBox::warning(this, QStringLiteral("资料读取失败"), message);
        return;
    }

    currentDocument_ = readResult;
    documentSignals_ = DocumentAnalyzer::analyzeDocument(currentDocument_);
    const auto candidates = DocumentAnalyzer::candidateItems(currentDocument_.text, documentSignals_);

    bool changed = false;
    for (const auto& candidate : candidates) {
        const bool exists = std::any_of(evidenceItems_.cbegin(), evidenceItems_.cend(), [&](const EvidenceItem& existing) {
            return sameCandidate(existing, candidate);
        });
        if (!exists) {
            evidenceItems_.append(candidate);
            changed = true;
        }
    }

    refreshCurrentDocumentSummary();
    refreshEvidenceTable();
    if (!currentDocument_.warning.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("PDF 读取提示"), currentDocument_.warning);
    }
    if (changed) emit evidenceChanged();
}

void EvidencePage::bindSelectedEvidence() {
    const int row = evidenceTable_->currentRow();
    if (row < 0 || row >= evidenceItems_.size()) {
        QMessageBox::information(this, QStringLiteral("绑定证据"), QStringLiteral("请先选择一条证据候选。"));
        return;
    }

    auto& item = evidenceItems_[row];
    item.boundCatalyst = catalystCombo_->currentData().toString();
    item.boundTimeHours = timeSpin_->value() < 0.0
        ? std::nullopt
        : std::optional<double>(timeSpin_->value());
    item.note = noteEdit_->text().trimmed();
    item.status = DocumentAnalyzer::bindingStatus(item.boundCatalyst, item.boundTimeHours);
    refreshEvidenceTable();
    evidenceTable_->selectRow(row);
    emit evidenceChanged();
}

void EvidencePage::unbindSelectedEvidence() {
    const int row = evidenceTable_->currentRow();
    if (row < 0 || row >= evidenceItems_.size()) {
        QMessageBox::information(this, QStringLiteral("解除绑定"), QStringLiteral("请先选择一条证据候选。"));
        return;
    }

    auto& item = evidenceItems_[row];
    item.boundCatalyst.clear();
    item.boundTimeHours = std::nullopt;
    item.note.clear();
    item.status = DocumentAnalyzer::bindingStatus(QString(), std::nullopt);
    refreshEvidenceTable();
    evidenceTable_->selectRow(row);
    emit evidenceChanged();
}

void EvidencePage::markConditionReviewed() {
    const int row = evidenceTable_->currentRow();
    if (row < 0 || row >= evidenceItems_.size()) {
        QMessageBox::information(this, QStringLiteral("条件复核"), QStringLiteral("请先选择一条证据候选。"));
        return;
    }

    auto& item = evidenceItems_[row];
    const QString selectedCatalyst = catalystCombo_->currentData().toString();
    const std::optional<double> selectedTime = timeSpin_->value() < 0.0
        ? std::nullopt
        : std::optional<double>(timeSpin_->value());
    const QString reviewNote = noteEdit_->text().trimmed();

    if (selectedCatalyst.isEmpty() || !selectedTime.has_value()) {
        QMessageBox::warning(
            this,
            QStringLiteral("条件复核"),
            QStringLiteral("条件复核前必须先选择明确的催化剂和时间点。"));
        return;
    }
    if (reviewNote.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("条件复核"),
            QStringLiteral("请填写复核备注，例如核对了温度、空速、压力、进料与指标定义。"));
        return;
    }

    item.boundCatalyst = selectedCatalyst;
    item.boundTimeHours = selectedTime;
    item.note = reviewNote;
    item.status = QStringLiteral("condition_reviewed_context_only");
    refreshEvidenceTable();
    evidenceTable_->selectRow(row);
    emit evidenceChanged();
}

void EvidencePage::returnSelectedToReview() {
    const int row = evidenceTable_->currentRow();
    if (row < 0 || row >= evidenceItems_.size()) {
        QMessageBox::information(this, QStringLiteral("退回复核"), QStringLiteral("请先选择一条证据候选。"));
        return;
    }

    auto& item = evidenceItems_[row];
    item.status = DocumentAnalyzer::bindingStatus(item.boundCatalyst, item.boundTimeHours);
    refreshEvidenceTable();
    evidenceTable_->selectRow(row);
    emit evidenceChanged();
}

void EvidencePage::resetCurrentDocumentSummary() {
    sourceLabel_->setText(QStringLiteral("尚未加载资料"));
    documentInfoLabel_->setText(QStringLiteral("—"));
    warningLabel_->clear();
    pageCountLabel_->setText(QStringLiteral("—"));
    characterLabel_->setText(QStringLiteral("—"));
    doiCountLabel_->setText(QStringLiteral("—"));
    temperatureCountLabel_->setText(QStringLiteral("—"));
    durationCountLabel_->setText(QStringLiteral("—"));
    signalsTable_->setRowCount(0);
}

void EvidencePage::refreshCurrentDocumentSummary() {
    sourceLabel_->setText(currentDocument_.sourcePath);
    documentInfoLabel_->setText(QStringLiteral("%1 · %2 页 · SHA-256 %3…")
        .arg(formatLabel(currentDocument_.sourceFormat))
        .arg(currentDocument_.pageCount)
        .arg(currentDocument_.sourceSha256.left(16)));
    warningLabel_->setText(currentDocument_.warning);
    pageCountLabel_->setText(QString::number(documentSignals_.pageCount));
    characterLabel_->setText(QString::number(documentSignals_.characterCount));
    doiCountLabel_->setText(QString::number(documentSignals_.dois.size()));
    temperatureCountLabel_->setText(QString::number(documentSignals_.temperaturesC.size()));
    durationCountLabel_->setText(QString::number(documentSignals_.durationsHours.size()));

    struct SignalRow { QString label; QString value; };
    QVector<SignalRow> rows;
    rows.append({QStringLiteral("DOI"), documentSignals_.dois.isEmpty()
        ? QStringLiteral("—") : documentSignals_.dois.join(QStringLiteral("，"))});
    rows.append({QStringLiteral("温度"), joinNumbers(documentSignals_.temperaturesC, QStringLiteral(" °C"))});
    rows.append({QStringLiteral("测试时长"), joinNumbers(documentSignals_.durationsHours, QStringLiteral(" h"))});
    rows.append({QStringLiteral("CH4 转化率候选"),
        joinNumbers(documentSignals_.ch4ConversionPercentCandidates, QStringLiteral("%"))});

    for (auto it = documentSignals_.keywordEvidence.cbegin(); it != documentSignals_.keywordEvidence.cend(); ++it) {
        QString category = it.key();
        if (category == QStringLiteral("coking")) category = QStringLiteral("积碳 / 结焦");
        else if (category == QStringLiteral("sintering")) category = QStringLiteral("烧结");
        else if (category == QStringLiteral("stability")) category = QStringLiteral("稳定 / 失活");
        else if (category == QStringLiteral("regeneration")) category = QStringLiteral("再生");
        rows.append({category, it.value().join(QStringLiteral("，"))});
    }

    signalsTable_->setRowCount(rows.size());
    for (qsizetype row = 0; row < rows.size(); ++row) {
        signalsTable_->setItem(row, 0, readOnlyItem(rows[row].label));
        signalsTable_->setItem(row, 1, readOnlyItem(rows[row].value));
    }
    signalsTable_->resizeRowsToContents();
}

void EvidencePage::refreshEvidenceTable() {
    evidenceCountLabel_->setText(QString::number(evidenceItems_.size()));
    evidenceTable_->setRowCount(evidenceItems_.size());
    for (qsizetype row = 0; row < evidenceItems_.size(); ++row) {
        const auto& item = evidenceItems_[row];
        evidenceTable_->setItem(row, 0, readOnlyItem(item.sourcePath.isEmpty()
            ? QStringLiteral("—") : QFileInfo(item.sourcePath).fileName()));
        evidenceTable_->setItem(row, 1, readOnlyItem(item.sourcePage > 0
            ? QString::number(item.sourcePage) : QStringLiteral("—")));
        evidenceTable_->setItem(row, 2, readOnlyItem(categoryLabel(item.category)));
        evidenceTable_->setItem(row, 3, readOnlyItem(displayCandidate(item)));
        auto* statusItem = readOnlyItem(statusLabel(item.status));
        if (item.status == QStringLiteral("condition_reviewed_context_only")) {
            statusItem->setForeground(QColor(QStringLiteral("#166534")));
            statusItem->setBackground(QColor(QStringLiteral("#F0FDF4")));
        } else if (item.status == QStringLiteral("candidate_requires_condition_binding")) {
            statusItem->setForeground(QColor(QStringLiteral("#71717A")));
            statusItem->setBackground(QColor(QStringLiteral("#F4F4F5")));
        } else {
            statusItem->setForeground(QColor(QStringLiteral("#92400E")));
            statusItem->setBackground(QColor(QStringLiteral("#FFFBEB")));
        }
        evidenceTable_->setItem(row, 4, statusItem);
        evidenceTable_->setItem(row, 5, readOnlyItem(item.boundCatalyst.isEmpty()
            ? QStringLiteral("—") : item.boundCatalyst));
        evidenceTable_->setItem(row, 6, readOnlyItem(item.boundTimeHours.has_value()
            ? QString::number(*item.boundTimeHours, 'g', 8) : QStringLiteral("—")));
        const QString context = item.note.isEmpty()
            ? item.snippet
            : QStringLiteral("%1\n备注：%2").arg(item.snippet, item.note);
        evidenceTable_->setItem(row, 7, readOnlyItem(context.isEmpty() ? QStringLiteral("—") : context));
    }
    evidenceTable_->resizeRowsToContents();
    refreshEvidencePacket();
}

void EvidencePage::refreshEvidencePacket() {
    const EvidencePacket packet = EvidencePacketBuilder::build(evidenceItems_);
    if (packet.readyForAi) {
        packetStatusLabel_->setText(QStringLiteral("可进入 AI：%1 条 · 排除 %2 条")
            .arg(packet.reviewedContextItems)
            .arg(packet.pendingItems));
        packetStatusLabel_->setStyleSheet(QString());
        packetStatusLabel_->setObjectName(QStringLiteral("statusGood"));
        packetStatusLabel_->style()->unpolish(packetStatusLabel_);
        packetStatusLabel_->style()->polish(packetStatusLabel_);
    } else {
        packetStatusLabel_->setText(QStringLiteral("暂不可进入 AI · 待复核 %1 条").arg(packet.pendingItems));
        packetStatusLabel_->setStyleSheet(QString());
        packetStatusLabel_->setObjectName(QStringLiteral("statusWarn"));
        packetStatusLabel_->style()->unpolish(packetStatusLabel_);
        packetStatusLabel_->style()->polish(packetStatusLabel_);
    }
    packetPreview_->setPlainText(EvidencePacketBuilder::toMarkdown(packet));
}

} // namespace catalyst
