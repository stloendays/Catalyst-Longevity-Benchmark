#include "chartwidget.h"

#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace catalyst {

ChartWidget::ChartWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(310);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ChartWidget::setRecords(const QVector<Record>& records) {
    records_ = records;
    update();
}

void ChartWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(QStringLiteral("#FFFFFF")));

    if (records_.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#6B7280")));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("导入数据后显示长期性能曲线"));
        return;
    }

    QMap<QString, QVector<Record>> grouped;
    double minTime = records_.front().timeHours;
    double maxTime = minTime;
    double minPerformance = records_.front().performance;
    double maxPerformance = minPerformance;
    for (const auto& record : records_) {
        grouped[record.catalyst].append(record);
        minTime = qMin(minTime, record.timeHours);
        maxTime = qMax(maxTime, record.timeHours);
        minPerformance = qMin(minPerformance, record.performance);
        maxPerformance = qMax(maxPerformance, record.performance);
    }
    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        std::sort(it.value().begin(), it.value().end(), [](const Record& a, const Record& b) {
            return a.timeHours < b.timeHours;
        });
    }

    if (qFuzzyCompare(minTime, maxTime)) {
        maxTime = minTime + 1.0;
    }
    if (qFuzzyCompare(minPerformance, maxPerformance)) {
        maxPerformance = minPerformance + 1.0;
    }

    const double performancePad = qMax(1.0, (maxPerformance - minPerformance) * 0.12);
    minPerformance -= performancePad;
    maxPerformance += performancePad;

    const QRectF plotRect = rect().adjusted(68, 30, -28, -58);
    const auto mapX = [&](double time) {
        return plotRect.left() + ((time - minTime) / (maxTime - minTime)) * plotRect.width();
    };
    const auto mapY = [&](double performance) {
        return plotRect.bottom() - ((performance - minPerformance) / (maxPerformance - minPerformance)) * plotRect.height();
    };

    painter.setPen(QPen(QColor(QStringLiteral("#E5E7EB")), 1));
    for (int i = 0; i <= 5; ++i) {
        const double ratio = static_cast<double>(i) / 5.0;
        const double y = plotRect.top() + ratio * plotRect.height();
        painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
        const double value = maxPerformance - ratio * (maxPerformance - minPerformance);
        painter.setPen(QColor(QStringLiteral("#6B7280")));
        painter.drawText(QRectF(2, y - 9, 58, 18), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(value, 'f', 1));
        painter.setPen(QPen(QColor(QStringLiteral("#E5E7EB")), 1));
    }

    painter.setPen(QPen(QColor(QStringLiteral("#9CA3AF")), 1.2));
    painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());
    painter.drawLine(plotRect.bottomLeft(), plotRect.topLeft());

    painter.setPen(QColor(QStringLiteral("#6B7280")));
    for (int i = 0; i <= 5; ++i) {
        const double ratio = static_cast<double>(i) / 5.0;
        const double x = plotRect.left() + ratio * plotRect.width();
        const double value = minTime + ratio * (maxTime - minTime);
        painter.drawText(QRectF(x - 35, plotRect.bottom() + 8, 70, 20), Qt::AlignCenter,
                         QString::number(value, 'g', 5));
    }
    painter.drawText(QRectF(plotRect.left(), plotRect.bottom() + 32, plotRect.width(), 20),
                     Qt::AlignCenter, QStringLiteral("Time on stream (h)"));

    const QList<QColor> colors = {
        QColor(QStringLiteral("#2563EB")), QColor(QStringLiteral("#DC2626")),
        QColor(QStringLiteral("#059669")), QColor(QStringLiteral("#7C3AED")),
        QColor(QStringLiteral("#D97706")), QColor(QStringLiteral("#0891B2")),
        QColor(QStringLiteral("#4F46E5")), QColor(QStringLiteral("#DB2777"))
    };

    int seriesIndex = 0;
    double legendX = plotRect.left();
    const double legendY = 8.0;
    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it, ++seriesIndex) {
        const QColor color = colors[seriesIndex % colors.size()];
        const auto& rows = it.value();
        QPainterPath path;
        for (qsizetype i = 0; i < rows.size(); ++i) {
            const QPointF point(mapX(rows[i].timeHours), mapY(rows[i].performance));
            if (i == 0) {
                path.moveTo(point);
            } else {
                path.lineTo(point);
            }
        }
        painter.setPen(QPen(color, 2.4));
        painter.drawPath(path);
        painter.setBrush(color);
        for (const auto& row : rows) {
            painter.drawEllipse(QPointF(mapX(row.timeHours), mapY(row.performance)), 4.0, 4.0);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(legendX + 5, legendY + 7), 4.5, 4.5);
        painter.setPen(QColor(QStringLiteral("#374151")));
        const QFontMetrics metrics(painter.font());
        const int textWidth = metrics.horizontalAdvance(it.key());
        painter.drawText(QRectF(legendX + 15, legendY, textWidth + 8, 16), Qt::AlignVCenter, it.key());
        legendX += textWidth + 38;
    }
}

} // namespace catalyst
