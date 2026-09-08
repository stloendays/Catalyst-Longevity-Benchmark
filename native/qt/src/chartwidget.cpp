#include "chartwidget.h"

#include <QFontMetrics>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace catalyst {

ChartWidget::ChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(330);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void ChartWidget::setRecords(const QVector<Record>& records) {
    records_ = records;
    hovered_.reset();
    update();
}

void ChartWidget::mouseMoveEvent(QMouseEvent* event) {
    constexpr double radius = 15.0;
    double bestDistance = radius;
    std::optional<HoverPoint> best;
    for (const auto& point : renderedPoints_) {
        const double dx = point.screen.x() - event->position().x();
        const double dy = point.screen.y() - event->position().y();
        const double distance = std::sqrt(dx * dx + dy * dy);
        if (distance < bestDistance) { bestDistance = distance; best = point; }
    }
    const bool changed = best.has_value() != hovered_.has_value()
        || (best && hovered_ && (best->catalyst != hovered_->catalyst
            || !qFuzzyCompare(best->timeHours + 1.0, hovered_->timeHours + 1.0)
            || !qFuzzyCompare(best->performance + 1.0, hovered_->performance + 1.0)));
    if (changed) { hovered_ = best; update(); }
    QWidget::mouseMoveEvent(event);
}

void ChartWidget::leaveEvent(QEvent* event) {
    hovered_.reset();
    update();
    QWidget::leaveEvent(event);
}

void ChartWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(QStringLiteral("#FFFFFF")));
    renderedPoints_.clear();

    if (records_.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#71717A")));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("导入数据后显示长期性能曲线"));
        return;
    }

    QMap<QString, QVector<Record>> grouped;
    double minTime = records_.front().timeHours, maxTime = minTime;
    double minPerformance = records_.front().performance, maxPerformance = minPerformance;
    for (const auto& record : records_) {
        grouped[record.catalyst].append(record);
        minTime = qMin(minTime, record.timeHours); maxTime = qMax(maxTime, record.timeHours);
        minPerformance = qMin(minPerformance, record.performance); maxPerformance = qMax(maxPerformance, record.performance);
    }
    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        std::sort(it.value().begin(), it.value().end(), [](const Record& a, const Record& b) { return a.timeHours < b.timeHours; });
    }
    if (qFuzzyCompare(minTime, maxTime)) maxTime = minTime + 1.0;
    if (qFuzzyCompare(minPerformance, maxPerformance)) maxPerformance = minPerformance + 1.0;
    const double pad = qMax(1.0, (maxPerformance - minPerformance) * 0.12);
    minPerformance -= pad; maxPerformance += pad;

    const QRectF plotRect = rect().adjusted(70, 42, -30, -62);
    const auto mapX = [&](double t) { return plotRect.left() + ((t - minTime) / (maxTime - minTime)) * plotRect.width(); };
    const auto mapY = [&](double v) { return plotRect.bottom() - ((v - minPerformance) / (maxPerformance - minPerformance)) * plotRect.height(); };

    painter.setPen(QPen(QColor(QStringLiteral("#ECECEE")), 1));
    for (int i = 0; i <= 5; ++i) {
        const double ratio = double(i) / 5.0;
        const double y = plotRect.top() + ratio * plotRect.height();
        painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
        painter.setPen(QColor(QStringLiteral("#71717A")));
        painter.drawText(QRectF(2, y - 9, 58, 18), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(maxPerformance - ratio * (maxPerformance - minPerformance), 'f', 1));
        painter.setPen(QPen(QColor(QStringLiteral("#ECECEE")), 1));
    }
    painter.setPen(QColor(QStringLiteral("#71717A")));
    for (int i = 0; i <= 5; ++i) {
        const double ratio = double(i) / 5.0;
        const double x = plotRect.left() + ratio * plotRect.width();
        painter.drawText(QRectF(x - 35, plotRect.bottom() + 9, 70, 20), Qt::AlignCenter,
                         QString::number(minTime + ratio * (maxTime - minTime), 'g', 5));
    }
    painter.drawText(QRectF(plotRect.left(), plotRect.bottom() + 34, plotRect.width(), 20),
                     Qt::AlignCenter, QStringLiteral("时间 / Time on stream (h)"));

    const QList<QColor> tones = { QColor("#111111"), QColor("#3F3F46"), QColor("#71717A"), QColor("#A1A1AA"), QColor("#52525B"), QColor("#27272A"), QColor("#8B8B92"), QColor("#18181B") };
    const QList<Qt::PenStyle> styles = { Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine, Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine };

    int seriesIndex = 0;
    double legendX = plotRect.left();
    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it, ++seriesIndex) {
        const QColor tone = tones[seriesIndex % tones.size()];
        const auto penStyle = styles[seriesIndex % styles.size()];
        QPainterPath path;
        const auto& rows = it.value();
        for (qsizetype i = 0; i < rows.size(); ++i) {
            const QPointF point(mapX(rows[i].timeHours), mapY(rows[i].performance));
            renderedPoints_.append({it.key(), rows[i].timeHours, rows[i].performance, point});
            if (i == 0) path.moveTo(point); else path.lineTo(point);
        }
        painter.setPen(QPen(tone, 2.2, penStyle, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush); painter.drawPath(path);
        for (const auto& row : rows) {
            const QPointF point(mapX(row.timeHours), mapY(row.performance));
            painter.setPen(QPen(tone, 1.8)); painter.setBrush(Qt::white); painter.drawEllipse(point, 4.2, 4.2);
        }
        painter.setPen(QPen(tone, 2.2, penStyle)); painter.drawLine(QPointF(legendX, 20), QPointF(legendX + 18, 20));
        painter.setPen(QColor("#52525B"));
        const int width = QFontMetrics(painter.font()).horizontalAdvance(it.key());
        painter.drawText(QRectF(legendX + 24, 12, width + 8, 17), Qt::AlignVCenter, it.key());
        legendX += width + 52;
    }

    if (hovered_) {
        const QPointF point = hovered_->screen;
        painter.setPen(QPen(QColor("#C7C7CC"), 1, Qt::DashLine));
        painter.drawLine(QPointF(point.x(), plotRect.top()), QPointF(point.x(), plotRect.bottom()));
        painter.drawLine(QPointF(plotRect.left(), point.y()), QPointF(plotRect.right(), point.y()));
        painter.setPen(QPen(QColor("#111111"), 2.2)); painter.setBrush(Qt::white); painter.drawEllipse(point, 6.2, 6.2);

        const QString title = hovered_->catalyst;
        const QString detail = QStringLiteral("%1 h   ·   %2").arg(
            QString::number(hovered_->timeHours, 'g', 6), QString::number(hovered_->performance, 'g', 7));
        const QFontMetrics fm(painter.font());
        const int width = qMax(fm.horizontalAdvance(title), fm.horizontalAdvance(detail)) + 28;
        const int height = 58;
        double x = point.x() + 14, y = point.y() - height - 12;
        if (x + width > rect().right() - 8) x = point.x() - width - 14;
        if (y < 8) y = point.y() + 14;
        const QRectF tip(x, y, width, height);
        painter.setPen(QPen(QColor("#D4D4D8"), 1)); painter.setBrush(Qt::white); painter.drawRoundedRect(tip, 9, 9);
        QFont bold = painter.font(); bold.setBold(true); painter.setFont(bold); painter.setPen(QColor("#111111"));
        painter.drawText(tip.adjusted(12, 8, -12, -30), Qt::AlignLeft | Qt::AlignVCenter, title);
        bold.setBold(false); painter.setFont(bold); painter.setPen(QColor("#71717A"));
        painter.drawText(tip.adjusted(12, 29, -12, -7), Qt::AlignLeft | Qt::AlignVCenter, detail);
    }
}

} // namespace catalyst
