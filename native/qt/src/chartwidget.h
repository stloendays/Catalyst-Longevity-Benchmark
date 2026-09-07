#pragma once

#include "models.h"

#include <QWidget>

namespace catalyst {

class ChartWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ChartWidget(QWidget* parent = nullptr);
    void setRecords(const QVector<Record>& records);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<Record> records_;
};

} // namespace catalyst
