#pragma once

#include "documentanalyzer.h"

#include <QWidget>

class QLabel;
class QTableWidget;

namespace catalyst {

class EvidencePage final : public QWidget {
    Q_OBJECT

public:
    explicit EvidencePage(QWidget* parent = nullptr);

private slots:
    void chooseDocument();

private:
    void refreshView();
    void clearView();

    QString sourcePath_;
    QString sourceText_;
    DocumentSignals signals_;

    QLabel* sourceLabel_ = nullptr;
    QLabel* characterLabel_ = nullptr;
    QLabel* doiCountLabel_ = nullptr;
    QLabel* temperatureCountLabel_ = nullptr;
    QLabel* durationCountLabel_ = nullptr;
    QLabel* keywordCountLabel_ = nullptr;
    QTableWidget* signalsTable_ = nullptr;
    QTableWidget* snippetsTable_ = nullptr;
};

} // namespace catalyst
