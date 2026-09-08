#pragma once

#include "documentanalyzer.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace catalyst {

class EvidencePage final : public QWidget {
    Q_OBJECT

public:
    explicit EvidencePage(QWidget* parent = nullptr);

    [[nodiscard]] QVector<EvidenceItem> evidenceItems() const;
    void setEvidenceItems(const QVector<EvidenceItem>& items);
    void clearEvidence();
    void setCatalystNames(const QStringList& catalystNames);

signals:
    void evidenceChanged();

private slots:
    void chooseDocument();
    void bindSelectedEvidence();
    void unbindSelectedEvidence();

private:
    void refreshCurrentDocumentSummary();
    void refreshEvidenceTable();
    void resetCurrentDocumentSummary();

    QString sourcePath_;
    QString sourceText_;
    DocumentSignals documentSignals_;
    QVector<EvidenceItem> evidenceItems_;

    QLabel* sourceLabel_ = nullptr;
    QLabel* characterLabel_ = nullptr;
    QLabel* doiCountLabel_ = nullptr;
    QLabel* temperatureCountLabel_ = nullptr;
    QLabel* durationCountLabel_ = nullptr;
    QLabel* evidenceCountLabel_ = nullptr;
    QTableWidget* signalsTable_ = nullptr;
    QTableWidget* evidenceTable_ = nullptr;
    QComboBox* catalystCombo_ = nullptr;
    QDoubleSpinBox* timeSpin_ = nullptr;
    QLineEdit* noteEdit_ = nullptr;
};

} // namespace catalyst
