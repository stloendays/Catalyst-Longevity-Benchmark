#include "chartwidget.h"

#include <QFontMetrics>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStringList>
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
    selectedCatalyst_.clear();
    update();
}

std::optional<ChartWidget::HoverPoint> ChartWidget::nearestPoint(const QPointF& position, double radius) const {
    double bestDistance = radius;
    std::optional<HoverPoint> best;
    for (const auto& point : renderedPoints_) {
        const double dx = point.screen.x() - position.x();
        const double dy = point.screen.y() - position.y();
        const double distance = std::sqrt(dx * dx + dy * dy);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = point;
        }
    }
    return best;
}

void ChartWidget::mouseMoveEvent(QMouseEvent* event) {
    const auto best = nearestPoint(event->position(), 15.0);
    const bool changed = best.has_value() != hovered_.has_value()
        || (best && hovered_ && (best->catalyst != hovered_->catalyst
            || !qFuzzyCompare(best->timeHours + 1.0, hovered_->timeHours + 1.0)
            || !qFuzzyCompare(best->performance + 1.0, hovered_->performance + 1.0)));
    if (changed) {
        hovered_ = best;
        update();
    }
    setCursor(best ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
}

void ChartWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const auto point = nearestPoint(event->position(), 18.0);
        if (point) {
            selectedCatalyst_ = selectedCatalyst_ == point->catalyst ? QString() : point->catalyst;
        } else {
            selectedCatalyst_.clear();
        }
        update();
    }
    QWidget::mousePressEvent(event);
}

void ChartWidget::leaveEvent(QEvent* event) {
    hovered_.reset();
    setCursor(Qt::ArrowCursor);
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

    if (qFuzzyCompare(minTime, maxTime)) maxTime = minTime + 1.0;
    if (qFuzzyCompare(minPerformance, maxPerformance)) maxPerformance = minPerformance + 1.0;
    const double pad = qMax(1.0, (maxPerformance - minPerformance) * 0.12);
    minPerformance -= pad;
    maxPerformance += pad;

    const QRectF plotRect = rect().adjusted(70, 48, -30, -62);
    const auto mapX = [&](double t) {
        return plotRect.left() + ((t - minTime) / (maxTime - minTime)) * plotRect.width();
    };
    const auto mapY = [&](double v) {
        return plotRect.bottom() - ((v - minPerformance) / (maxPerformance - minPerformance)) * plotRect.height();
    };

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

    const QList<QColor> tones = {
        QColor(QStringLiteral("#111111")), QColor(QStringLiteral("#3F3F46")),
        QColor(QStringLiteral("#71717A")), QColor(QStringLiteral("#A1A1AA")),
        QColor(QStringLiteral("#52525B")), QColor(QStringLiteral("#27272A")),
        QColor(QStringLiteral("#8B8B92")), QColor(QStringLiteral("#18181B"))
    };
    const QList<Qt::PenStyle> styles = {
        Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine,
        Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine
    };

    QStringList names = grouped.keys();
    if (!selectedCatalyst_.isEmpty() && names.contains(selectedCatalyst_)) {
        names.removeAll(selectedCatalyst_);
        names.append(selectedCatalyst_);
    }

    double legendX = plotRect.left();
    const QStringList originalNames = grouped.keys();
    for (const QString& name : names) {
        const int seriesIndex = originalNames.indexOf(name);
        const bool selected = !selectedCatalyst_.isEmpty() && name == selectedCatalyst_;
        const bool faded = !selectedCatalyst_.isEmpty() && !selected;
        const QColor tone = faded
            ? QColor(QStringLiteral("#D4D4D8"))
            : tones[seriesIndex % tones.size()];
        const auto penStyle = styles[seriesIndex % styles.size()];
        const double lineWidth = selected ? 3.0 : (faded ? 1.2 : 2.2);
        const auto& rows = grouped[name];

        QPainterPath path;
        for (qsizetype i = 0; i < rows.size(); ++i) {
            const QPointF point(mapX(rows[i].timeHours), mapY(rows[i].performance));
            renderedPoints_.append({name, rows[i].timeHours, rows[i].performance, point});
            if (i == 0) path.moveTo(point);
            else path.lineTo(point);
        }
        painter.setPen(QPen(tone, lineWidth, penStyle, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        for (const auto& row : rows) {
            const QPointF point(mapX(row.timeHours), mapY(row.performance));
            painter.setPen(QPen(tone, selected ? 2.2 : 1.6));
            painter.setBrush(selected ? tone : QColor(QStringLiteral("#FFFFFF")));
            painter.drawEllipse(point, selected ? 4.8 : 4.0, selected ? 4.8 : 4.0);
        }

        painter.setPen(QPen(tone, lineWidth, penStyle));
        painter.drawLine(QPointF(legendX, 20), QPointF(legendX + 18, 20));
        QFont legendFont = painter.font();
        legendFont.setBold(selected);
        painter.setFont(legendFont);
        painter.setPen(faded ? QColor(QStringLiteral("#A1A1AA")) : QColor(QStringLiteral("#52525B")));
        const int width = QFontMetrics(painter.font()).horizontalAdvance(name);
        painter.drawText(QRectF(legendX + 24, 12, width + 8, 17), Qt::AlignVCenter, name);
        legendX += width + 52;
        legendFont.setBold(false);
        painter.setFont(legendFont);
    }

    const QString focusText = selectedCatalyst_.isEmpty()
        ? QStringLiteral("单击曲线可聚焦")
        : QStringLiteral("当前：%1 · 再次单击取消").arg(selectedCatalyst_);
    painter.setPen(QColor(QStringLiteral("#71717A")));
    painter.drawText(QRectF(plotRect.right() - 320, 10, 320, 20), Qt::AlignRight | Qt::AlignVCenter, focusText);

    if (hovered_) {
        const QPointF point = hovered_->screen;
        painter.setPen(QPen(QColor(QStringLiteral("#C7C7CC")), 1, Qt::DashLine));
        painter.drawLine(QPointF(point.x(), plotRect.top()), QPointF(point.x(), plotRect.bottom()));
        painter.drawLine(QPointF(plotRect.left(), point.y()), QPointF(plotRect.right(), point.y()));
        painter.setPen(QPen(QColor(QStringLiteral("#111111")), 2.2));
        painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
        painter.drawEllipse(point, 6.2, 6.2);

        const QString title = hovered_->catalyst;
        const QString detail = QStringLiteral("%1 h   ·   %2")
            .arg(QString::number(hovered_->timeHours, 'g', 6),
                 QString::number(hovered_->performance, 'g', 7));
        const QFontMetrics fm(painter.font());
        const int width = qMax(fm.horizontalAdvance(title), fm.horizontalAdvance(detail)) + 28;
        const int height = 58;
        double x = point.x() + 14;
        double y = point.y() - height - 12;
        if (x + width > rect().right() - 8) x = point.x() - width - 14;
        if (y < 8) y = point.y() + 14;
        const QRectF tip(x, y, width, height);

        painter.setPen(QPen(QColor(QStringLiteral("#D4D4D8")), 1));
        painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
        painter.drawRoundedRect(tip, 9, 9);
        QFont bold = painter.font();
        bold.setBold(true);
        painter.setFont(bold);
        painter.setPen(QColor(QStringLiteral("#111111")));
        painter.drawText(tip.adjusted(12, 8, -12, -30), Qt::AlignLeft | Qt::AlignVCenter, title);
        bold.setBold(false);
        painter.setFont(bold);
        painter.setPen(QColor(QStringLiteral("#71717A")));
        painter.drawText(tip.adjusted(12, 29, -12, -7), Qt::AlignLeft | Qt::AlignVCenter, detail);
    }
}

} // namespace catalyst
