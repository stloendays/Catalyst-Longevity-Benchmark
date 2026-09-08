#pragma once

#include "models.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPointF>
#include <QWidget>
#include <optional>

namespace catalyst {

class ChartWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ChartWidget(QWidget* parent = nullptr);
    void setRecords(const QVector<Record>& records);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct HoverPoint {
        QString catalyst;
        double timeHours = 0.0;
        double performance = 0.0;
        QPointF screen;
    };
    QVector<Record> records_;
    QVector<HoverPoint> renderedPoints_;
    std::optional<HoverPoint> hovered_;
};

} // namespace catalyst
