#include "evidencepage.h"

#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

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
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("metricTitle"));
    auto* value = new QLabel(QStringLiteral("—"));
    value->setObjectName(QStringLiteral("metricValue"));
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
    if (values.isEmpty()) {
        return QStringLiteral("—");
    }
    QStringList text;
    const qsizetype limit = qMin<qsizetype>(values.size(), 20);
    text.reserve(limit);
    for (qsizetype i = 0; i < limit; ++i) {
        text.append(QStringLiteral("%1%2").arg(QString::number(values[i], 'g', 8), suffix));
    }
    if (values.size() > limit) {
        text.append(QStringLiteral("… 共 %1 项").arg(values.size()));
    }
    return text.join(QStringLiteral("，"));
}

} // namespace

EvidencePage::EvidencePage(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(34, 28, 34, 28);
    layout->setSpacing(16);

    auto* top = new QHBoxLayout;
    auto* titleBox = new QVBoxLayout;
    titleBox->addWidget(headingLabel(QStringLiteral("资料分析与证据提取")));
    titleBox->addWidget(mutedLabel(QStringLiteral(
        "保守提取 DOI、温度、测试时长、CH4 转化率候选值与失活证据词。孤立数值不会自动进入催化剂排名，仍需绑定到具体催化剂、时间和实验条件。")));
    top->addLayout(titleBox, 1);

    auto* chooseButton = new QPushButton(QStringLiteral("选择资料文件"));
    chooseButton->setObjectName(QStringLiteral("primaryButton"));
    connect(chooseButton, &QPushButton::clicked, this, &EvidencePage::chooseDocument);
    top->addWidget(chooseButton);
    layout->addLayout(top);

    auto* sourceFrame = new QFrame;
    sourceFrame->setObjectName(QStringLiteral("panel"));
    auto* sourceLayout = new QVBoxLayout(sourceFrame);
    sourceLayout->setContentsMargins(18, 14, 18, 14);
    auto* sourceTitle = new QLabel(QStringLiteral("当前资料"));
    sourceTitle->setObjectName(QStringLiteral("sectionTitle"));
    sourceLabel_ = new QLabel(QStringLiteral("尚未加载资料"));
    sourceLabel_->setObjectName(QStringLiteral("sourcePath"));
    sourceLabel_->setWordWrap(true);
    sourceLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sourceLayout->addWidget(sourceTitle);
    sourceLayout->addWidget(sourceLabel_);
    layout->addWidget(sourceFrame);

    auto* metrics = new QGridLayout;
    metrics->setHorizontalSpacing(12);
    metrics->addWidget(metricCard(QStringLiteral("字符数"), &characterLabel_), 0, 0);
    metrics->addWidget(metricCard(QStringLiteral("DOI"), &doiCountLabel_), 0, 1);
    metrics->addWidget(metricCard(QStringLiteral("温度候选"), &temperatureCountLabel_), 0, 2);
    metrics->addWidget(metricCard(QStringLiteral("时长候选"), &durationCountLabel_), 0, 3);
    metrics->addWidget(metricCard(QStringLiteral("证据类别"), &keywordCountLabel_), 0, 4);
    layout->addLayout(metrics);

    auto* signalFrame = new QFrame;
    signalFrame->setObjectName(QStringLiteral("panel"));
    auto* signalLayout = new QVBoxLayout(signalFrame);
    signalLayout->setContentsMargins(18, 16, 18, 16);
    auto* signalTitle = new QLabel(QStringLiteral("保守提取结果"));
    signalTitle->setObjectName(QStringLiteral("sectionTitle"));
    signalLayout->addWidget(signalTitle);

    signalsTable_ = new QTableWidget(0, 2);
    signalsTable_->setHorizontalHeaderLabels({QStringLiteral("类别"), QStringLiteral("候选信息")});
    signalsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    signalsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    signalsTable_->verticalHeader()->setVisible(false);
    signalsTable_->setAlternatingRowColors(true);
    signalLayout->addWidget(signalsTable_);
    layout->addWidget(signalFrame, 1);

    auto* snippetFrame = new QFrame;
    snippetFrame->setObjectName(QStringLiteral("panel"));
    auto* snippetLayout = new QVBoxLayout(snippetFrame);
    snippetLayout->setContentsMargins(18, 16, 18, 16);
    auto* snippetTitle = new QLabel(QStringLiteral("原文证据片段"));
    snippetTitle->setObjectName(QStringLiteral("sectionTitle"));
    snippetLayout->addWidget(snippetTitle);

    snippetsTable_ = new QTableWidget(0, 2);
    snippetsTable_->setHorizontalHeaderLabels({QStringLiteral("触发词"), QStringLiteral("原文上下文")});
    snippetsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    snippetsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    snippetsTable_->verticalHeader()->setVisible(false);
    snippetsTable_->setAlternatingRowColors(true);
    snippetsTable_->setWordWrap(true);
    snippetsTable_->setMinimumHeight(190);
    snippetLayout->addWidget(snippetsTable_);
    layout->addWidget(snippetFrame, 1);

    auto* note = new QFrame;
    note->setObjectName(QStringLiteral("infoPanel"));
    auto* noteLayout = new QVBoxLayout(note);
    noteLayout->addWidget(new QLabel(QStringLiteral("证据门槛")));
    noteLayout->addWidget(mutedLabel(QStringLiteral(
        "当前原生解析支持 TXT、Markdown、CSV、TSV。数值与关键词只是 source-locatable candidates；只有绑定到明确催化剂、时间点和条件后才允许进入后续排名。扫描 PDF 不会自动 OCR，以避免静默误读。")));
    layout->addWidget(note);

    clearView();
}

void EvidencePage::chooseDocument() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择资料文件"),
        QString(),
        QStringLiteral("文本资料 (*.txt *.md *.csv *.tsv);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    QString text;
    QString message;
    if (!DocumentAnalyzer::readTextFile(path, &text, &message)) {
        QMessageBox::warning(this, QStringLiteral("资料读取失败"), message);
        return;
    }

    sourcePath_ = path;
    sourceText_ = text;
    signals_ = DocumentAnalyzer::analyzeText(sourceText_, sourcePath_);
    refreshView();
}

void EvidencePage::clearView() {
    sourcePath_.clear();
    sourceText_.clear();
    signals_ = DocumentSignals{};
    if (sourceLabel_) sourceLabel_->setText(QStringLiteral("尚未加载资料"));
    if (characterLabel_) characterLabel_->setText(QStringLiteral("—"));
    if (doiCountLabel_) doiCountLabel_->setText(QStringLiteral("—"));
    if (temperatureCountLabel_) temperatureCountLabel_->setText(QStringLiteral("—"));
    if (durationCountLabel_) durationCountLabel_->setText(QStringLiteral("—"));
    if (keywordCountLabel_) keywordCountLabel_->setText(QStringLiteral("—"));
    if (signalsTable_) signalsTable_->setRowCount(0);
    if (snippetsTable_) snippetsTable_->setRowCount(0);
}

void EvidencePage::refreshView() {
    sourceLabel_->setText(sourcePath_);
    characterLabel_->setText(QString::number(signals_.characterCount));
    doiCountLabel_->setText(QString::number(signals_.dois.size()));
    temperatureCountLabel_->setText(QString::number(signals_.temperaturesC.size()));
    durationCountLabel_->setText(QString::number(signals_.durationsHours.size()));
    keywordCountLabel_->setText(QString::number(signals_.keywordEvidence.size()));

    struct SignalRow {
        QString label;
        QString value;
    };

    QVector<SignalRow> rows;
    rows.append({QStringLiteral("DOI"), signals_.dois.isEmpty() ? QStringLiteral("—") : signals_.dois.join(QStringLiteral("，"))});
    rows.append({QStringLiteral("温度"), joinNumbers(signals_.temperaturesC, QStringLiteral(" °C"))});
    rows.append({QStringLiteral("测试时长"), joinNumbers(signals_.durationsHours, QStringLiteral(" h"))});
    rows.append({QStringLiteral("CH4 转化率候选"), joinNumbers(signals_.ch4ConversionPercentCandidates, QStringLiteral("%"))});

    for (auto it = signals_.keywordEvidence.cbegin(); it != signals_.keywordEvidence.cend(); ++it) {
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

    snippetsTable_->setRowCount(signals_.snippets.size());
    for (qsizetype row = 0; row < signals_.snippets.size(); ++row) {
        snippetsTable_->setItem(row, 0, readOnlyItem(signals_.snippets[row].term));
        snippetsTable_->setItem(row, 1, readOnlyItem(signals_.snippets[row].snippet));
    }
    snippetsTable_->resizeRowsToContents();
}

} // namespace catalyst
