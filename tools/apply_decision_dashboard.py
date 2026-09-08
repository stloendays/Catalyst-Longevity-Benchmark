from pathlib import Path
import struct
import zlib


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing pattern: {label}")
    return text.replace(old, new, 1)


def restyle_function(text: str, signature: str, next_signature: str, replacement: str) -> str:
    start = text.index(signature)
    end = text.index(next_signature, start)
    return text[:start] + replacement + "\n\n" + text[end:]


# -----------------------------------------------------------------------------
# MainWindow header: dashboard decision state + AI evidence state.
# -----------------------------------------------------------------------------
p = Path("native/qt/src/mainwindow.h")
text = p.read_text(encoding="utf-8")
text = replace_once(
    text,
    "    void refreshAnalysisViews();\n",
    "    void refreshAnalysisViews();\n    void refreshDecisionOverview();\n",
    "refreshDecisionOverview declaration",
)
text = replace_once(
    text,
    "    QLabel* metricCondition_ = nullptr;\n",
    "    QLabel* metricCondition_ = nullptr;\n"
    "    QLabel* decisionComparability_ = nullptr;\n"
    "    QLabel* decisionComparabilityDetail_ = nullptr;\n"
    "    QLabel* decisionLeader_ = nullptr;\n"
    "    QLabel* decisionLeaderDetail_ = nullptr;\n"
    "    QLabel* decisionT90_ = nullptr;\n"
    "    QLabel* decisionT90Detail_ = nullptr;\n"
    "    QLabel* decisionEvidence_ = nullptr;\n"
    "    QLabel* decisionEvidenceDetail_ = nullptr;\n"
    "    QLabel* aiEvidenceStatus_ = nullptr;\n"
    "    QLabel* aiEvidenceDetail_ = nullptr;\n"
    "    QLabel* aiPacketStageStatus_ = nullptr;\n",
    "decision dashboard members",
)
p.write_text(text, encoding="utf-8")


# -----------------------------------------------------------------------------
# MainWindow implementation.
# -----------------------------------------------------------------------------
p = Path("native/qt/src/mainwindow.cpp")
text = p.read_text(encoding="utf-8")

# Decision-card visual hierarchy.
text = replace_once(
    text,
    "        #metricCard:hover { background:#FCFCFC; border-color:#CFCFD2; }\n",
    "        #metricCard:hover { background:#FCFCFC; border-color:#CFCFD2; }\n"
    "        #decisionCard { background:#FFFFFF; border:1px solid #E4E4E7; border-radius:14px; min-height:105px; }\n"
    "        #decisionCard:hover { background:#FCFCFC; border-color:#B8B8BE; }\n"
    "        #decisionTitle { color:#52525B; font-size:12px; font-weight:650; }\n"
    "        #decisionDetail { color:#71717A; font-size:12px; }\n",
    "decision card qss",
)

# Helper functions placed next to metricCard.
insert_marker = "QTableWidgetItem* readOnlyItem(const QString& text) {"
helpers = r'''QFrame* decisionCard(const QString& title, QLabel** statusLabel, QLabel** detailLabel) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("decisionCard"));
    card->setAttribute(Qt::WA_Hover, true);
    card->setMouseTracking(true);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(7);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("decisionTitle"));
    auto* state = new QLabel(QStringLiteral("等待数据"));
    state->setObjectName(QStringLiteral("statusNeutral"));
    auto* detail = new QLabel(QStringLiteral("尚未形成可审计判断。"));
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

'''
text = replace_once(text, insert_marker, helpers + insert_marker, "decision helpers")

# Evidence updates must refresh the dashboard and AI readiness.
text = replace_once(
    text,
    "        projectDirty_ = true;\n        updateProjectUi();\n        setStatus(QStringLiteral(\"证据候选已更新；保存项目可持久化当前绑定与复核状态。\"));\n",
    "        projectDirty_ = true;\n        updateProjectUi();\n        refreshDecisionOverview();\n        setStatus(QStringLiteral(\"证据候选已更新；保存项目可持久化当前绑定与复核状态。\"));\n",
    "evidence refresh hook",
)

# Dashboard: add the four decision-readiness cards above the numeric metrics.
metrics_marker = '''    auto* metrics = new QGridLayout;
    metrics->setHorizontalSpacing(12);'''
decision_block = '''    auto* decisionTop = new QHBoxLayout;
    auto* decisionHeading = new QLabel(QStringLiteral("决策状态  ·  Decision readiness"));
    decisionHeading->setObjectName(QStringLiteral("sectionTitle"));
    decisionTop->addWidget(decisionHeading);
    decisionTop->addStretch();
    decisionTop->addWidget(muted(QStringLiteral("先判断能不能比，再看谁领先。状态色只表达证据就绪度，不表达催化剂优劣。")));
    layout->addLayout(decisionTop);

    auto* decisions = new QGridLayout;
    decisions->setHorizontalSpacing(12);
    decisions->setVerticalSpacing(12);
    decisions->addWidget(decisionCard(QStringLiteral("实验条件可比性"), &decisionComparability_, &decisionComparabilityDetail_), 0, 0);
    decisions->addWidget(decisionCard(QStringLiteral("共同时间领先者"), &decisionLeader_, &decisionLeaderDetail_), 0, 1);
    decisions->addWidget(decisionCard(QStringLiteral("T90 是否测到"), &decisionT90_, &decisionT90Detail_), 0, 2);
    decisions->addWidget(decisionCard(QStringLiteral("证据包就绪度"), &decisionEvidence_, &decisionEvidenceDetail_), 0, 3);
    layout->addLayout(decisions);

    auto* metrics = new QGridLayout;
    metrics->setHorizontalSpacing(12);'''
text = replace_once(text, metrics_marker, decision_block, "decision dashboard block")

# Chart header explains interaction instead of making the behavior invisible.
text = replace_once(
    text,
    '''    auto* chartTitle = new QLabel(QStringLiteral("长期性能轨迹  ·  Long-term performance"));
    chartTitle->setObjectName(QStringLiteral("sectionTitle"));
    chart_ = new ChartWidget;
    chartLayout->addWidget(chartTitle);
    chartLayout->addWidget(chart_, 1);''',
    '''    auto* chartHead = new QHBoxLayout;
    auto* chartTitle = new QLabel(QStringLiteral("长期性能轨迹  ·  Long-term performance"));
    chartTitle->setObjectName(QStringLiteral("sectionTitle"));
    auto* chartHint = muted(QStringLiteral("悬浮查看精确点 · 单击数据点聚焦曲线 · 再次单击取消"));
    chartHead->addWidget(chartTitle);
    chartHead->addStretch();
    chartHead->addWidget(chartHint);
    chart_ = new ChartWidget;
    chartLayout->addLayout(chartHead);
    chartLayout->addWidget(chart_, 1);''',
    "chart interaction hint",
)

# AI readiness is no longer a static placeholder: bind it to the Evidence Packet.
text = replace_once(
    text,
    '''    auto* readinessState = new QLabel(QStringLiteral("本地证据链已启用"));
    readinessState->setObjectName(QStringLiteral("statusGood"));
    readinessLayout->addWidget(readinessTitle);
    readinessLayout->addWidget(muted(QStringLiteral("实验数据、资料候选与人工复核状态保持分层。")), 1);
    readinessLayout->addWidget(readinessState);''',
    '''    aiEvidenceDetail_ = muted(QStringLiteral("实验数据、资料候选与人工复核状态保持分层。"));
    aiEvidenceStatus_ = new QLabel(QStringLiteral("等待证据"));
    aiEvidenceStatus_->setObjectName(QStringLiteral("statusNeutral"));
    readinessLayout->addWidget(readinessTitle);
    readinessLayout->addWidget(aiEvidenceDetail_, 1);
    readinessLayout->addWidget(aiEvidenceStatus_);''',
    "AI evidence readiness binding",
)
text = replace_once(
    text,
    '''        auto* state = new QLabel(states[i]);
        state->setObjectName(i == 0 || i == 3 ? QStringLiteral("statusNeutral") : QStringLiteral("statusWarn"));''',
    '''        auto* state = new QLabel(states[i]);
        state->setObjectName(i == 0 || i == 3 ? QStringLiteral("statusNeutral") : QStringLiteral("statusWarn"));
        if (i == 0) aiPacketStageStatus_ = state;''',
    "AI stage status binding",
)

# The old built-in theme is no longer allowed to paint a blue frame before the deferred style arrives.
apply_theme = '''void MainWindow::applyTheme() {
    setStyleSheet(gptMonochromeStyle());
}'''
text = restyle_function(text, "void MainWindow::applyTheme() {", "void MainWindow::newProject() {", apply_theme)

# Replace the analysis condition inline color with semantic state chips.
old_condition = '''    if (conditionStatusLabel_) {
        conditionStatusLabel_->setText(ConditionGuard::statusText(analysis_.conditionAudit.status));
        conditionStatusLabel_->setStyleSheet(analysis_.conditionAudit.blocksDirectRanking()
            ? QStringLiteral("color: #B91C1C; font-size: 15px; font-weight: 700; padding: 3px 0;")
            : QStringLiteral("color: #166534; font-size: 15px; font-weight: 700; padding: 3px 0;"));
    }'''
new_condition = '''    if (conditionStatusLabel_) {
        QString conditionStyle = QStringLiteral("statusNeutral");
        if (analysis_.conditionAudit.status == ConditionAuditStatus::MatchedOnProvidedConditions) {
            conditionStyle = QStringLiteral("statusGood");
        } else if (analysis_.conditionAudit.status == ConditionAuditStatus::ConditionsNotProvided) {
            conditionStyle = QStringLiteral("statusWarn");
        } else if (analysis_.conditionAudit.blocksDirectRanking()) {
            conditionStyle = QStringLiteral("statusBad");
        }
        setStatusChip(conditionStatusLabel_, ConditionGuard::statusText(analysis_.conditionAudit.status), conditionStyle);
    }'''
text = replace_once(text, old_condition, new_condition, "condition state chip")

# Insert refreshDecisionOverview at the end of refreshAnalysisViews.
start = text.index("void MainWindow::refreshAnalysisViews() {")
end = text.index("void MainWindow::updateProjectUi() {", start)
block = text[start:end]
close = block.rfind("}\n")
if close < 0:
    raise RuntimeError("refreshAnalysisViews closing brace not found")
block = block[:close] + "    refreshDecisionOverview();\n" + block[close:]
text = text[:start] + block + text[end:]

# Live decision readiness and AI Evidence Packet state.
decision_fn = r'''void MainWindow::refreshDecisionOverview() {
    if (!decisionComparability_) return;

    if (analysis_.totalObservations <= 0) {
        setStatusChip(decisionComparability_, QStringLiteral("等待数据"), QStringLiteral("statusNeutral"));
        decisionComparabilityDetail_->setText(QStringLiteral("导入实验数据后检查温度、空速、压力和进料条件。"));
    } else if (analysis_.conditionAudit.blocksDirectRanking()) {
        setStatusChip(decisionComparability_, QStringLiteral("禁止直接排名"), QStringLiteral("statusBad"));
        decisionComparabilityDetail_->setText(QStringLiteral("检测到明确条件错配；需要先处理条件差异。"));
    } else if (analysis_.conditionAudit.status == ConditionAuditStatus::MatchedOnProvidedConditions) {
        setStatusChip(decisionComparability_, QStringLiteral("允许直接比较"), QStringLiteral("statusGood"));
        decisionComparabilityDetail_->setText(QStringLiteral("已提供的实验条件一致，可继续查看共同时间表现。"));
    } else {
        setStatusChip(decisionComparability_, QStringLiteral("条件信息不足"), QStringLiteral("statusWarn"));
        decisionComparabilityDetail_->setText(QStringLiteral("未发现明确冲突，但条件字段不足以形成强可比性结论。"));
    }

    if (analysis_.totalObservations <= 0) {
        setStatusChip(decisionLeader_, QStringLiteral("等待分析"), QStringLiteral("statusNeutral"));
        decisionLeaderDetail_->setText(QStringLiteral("领先者只在共同可比时间点上计算。"));
    } else if (analysis_.conditionAudit.blocksDirectRanking()) {
        setStatusChip(decisionLeader_, QStringLiteral("排名已阻止"), QStringLiteral("statusBad"));
        decisionLeaderDetail_->setText(QStringLiteral("条件守门优先于性能排序。"));
    } else if (analysis_.latestSharedTimeHours.has_value() && !analysis_.latestSharedLeader.isEmpty()) {
        setStatusChip(decisionLeader_, analysis_.latestSharedLeader, QStringLiteral("statusGood"));
        decisionLeaderDetail_->setText(QStringLiteral("共同时间 %1 h；此状态不外推到未观测时间。")
            .arg(QString::number(*analysis_.latestSharedTimeHours, 'g', 8)));
    } else {
        setStatusChip(decisionLeader_, QStringLiteral("暂无共同时间点"), QStringLiteral("statusWarn"));
        decisionLeaderDetail_->setText(QStringLiteral("当前轨迹无法在同一观测时间形成直接领先判断。"));
    }

    const int totalCatalysts = analysis_.catalysts.size();
    int t90Touched = 0;
    for (const auto& summary : analysis_.catalysts) {
        if (summary.t90.status != ThresholdStatus::RightCensored) ++t90Touched;
    }
    if (totalCatalysts == 0) {
        setStatusChip(decisionT90_, QStringLiteral("等待数据"), QStringLiteral("statusNeutral"));
        decisionT90Detail_->setText(QStringLiteral("T90 保留左删失、区间删失与右删失语义。"));
    } else if (t90Touched == totalCatalysts) {
        setStatusChip(decisionT90_, QStringLiteral("%1/%2 已触及").arg(t90Touched).arg(totalCatalysts), QStringLiteral("statusGood"));
        decisionT90Detail_->setText(QStringLiteral("所有催化剂均已观测到 T90 阈值通过区间。"));
    } else if (t90Touched > 0) {
        setStatusChip(decisionT90_, QStringLiteral("%1/%2 已触及").arg(t90Touched).arg(totalCatalysts), QStringLiteral("statusWarn"));
        decisionT90Detail_->setText(QStringLiteral("%1 个仍为右删失：测试结束时尚未跌破 90%。").arg(totalCatalysts - t90Touched));
    } else {
        setStatusChip(decisionT90_, QStringLiteral("尚未触及 T90"), QStringLiteral("statusWarn"));
        decisionT90Detail_->setText(QStringLiteral("全部轨迹仍为右删失；这表示当前测试只给出寿命下界。"));
    }

    if (!evidencePage_) return;
    const EvidencePacket packet = evidencePage_->evidencePacket();
    if (packet.readyForAi) {
        setStatusChip(decisionEvidence_, QStringLiteral("可进入 AI · %1 条").arg(packet.reviewedContextItems), QStringLiteral("statusGood"));
        decisionEvidenceDetail_->setText(QStringLiteral("另有 %1 条尚未满足证据门槛。").arg(packet.pendingItems));
        setStatusChip(aiEvidenceStatus_, QStringLiteral("证据包可用 · %1 条").arg(packet.reviewedContextItems), QStringLiteral("statusGood"));
        setStatusChip(aiPacketStageStatus_, QStringLiteral("可审计 · %1 条").arg(packet.reviewedContextItems), QStringLiteral("statusGood"));
        if (aiEvidenceDetail_) aiEvidenceDetail_->setText(QStringLiteral("已形成受控 Evidence Packet；AI 只能读取通过人工条件复核的上下文。"));
    } else if (packet.pendingItems > 0) {
        setStatusChip(decisionEvidence_, QStringLiteral("待复核 · %1 条").arg(packet.pendingItems), QStringLiteral("statusWarn"));
        decisionEvidenceDetail_->setText(QStringLiteral("完成催化剂、时间点与条件复核后才能进入 AI。"));
        setStatusChip(aiEvidenceStatus_, QStringLiteral("待复核 · %1 条").arg(packet.pendingItems), QStringLiteral("statusWarn"));
        setStatusChip(aiPacketStageStatus_, QStringLiteral("尚未就绪"), QStringLiteral("statusWarn"));
        if (aiEvidenceDetail_) aiEvidenceDetail_->setText(QStringLiteral("候选证据已存在，但 Evidence Packet 尚未满足受控输入门槛。"));
    } else {
        setStatusChip(decisionEvidence_, QStringLiteral("尚无证据"), QStringLiteral("statusNeutral"));
        decisionEvidenceDetail_->setText(QStringLiteral("在“资料证据”页导入论文并完成绑定与人工条件复核。"));
        setStatusChip(aiEvidenceStatus_, QStringLiteral("等待证据"), QStringLiteral("statusNeutral"));
        setStatusChip(aiPacketStageStatus_, QStringLiteral("等待证据"), QStringLiteral("statusNeutral"));
        if (aiEvidenceDetail_) aiEvidenceDetail_->setText(QStringLiteral("尚未形成可供 AI 使用的 Evidence Packet。"));
    }
}

'''
text = replace_once(text, "void MainWindow::updateProjectUi() {", decision_fn + "void MainWindow::updateProjectUi() {", "decision refresh function")

p.write_text(text, encoding="utf-8")


# -----------------------------------------------------------------------------
# Interactive chart: hover tooltip + click-to-focus + faded non-selected series.
# -----------------------------------------------------------------------------
Path("native/qt/src/chartwidget.h").write_text(r'''#pragma once

#include "models.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPointF>
#include <QString>
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
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct HoverPoint {
        QString catalyst;
        double timeHours = 0.0;
        double performance = 0.0;
        QPointF screen;
    };

    std::optional<HoverPoint> nearestPoint(const QPointF& position, double radius) const;

    QVector<Record> records_;
    QVector<HoverPoint> renderedPoints_;
    std::optional<HoverPoint> hovered_;
    QString selectedCatalyst_;
};

} // namespace catalyst
''', encoding="utf-8")

Path("native/qt/src/chartwidget.cpp").write_text(r'''#include "chartwidget.h"

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
        ? QStringLiteral("单击数据点聚焦")
        : QStringLiteral("已聚焦：%1 · 单击同一曲线取消").arg(selectedCatalyst_);
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
''', encoding="utf-8")


# -----------------------------------------------------------------------------
# Launcher polish: no teal style override; monochrome runtime icon.
# -----------------------------------------------------------------------------
p = Path("native/qt/src/main.cpp")
text = p.read_text(encoding="utf-8")
text = text.replace(
    'gradient.setColorAt(0.0, QColor(QStringLiteral("#0F766E")));',
    'gradient.setColorAt(0.0, QColor(QStringLiteral("#111111")));',
)
text = text.replace(
    'gradient.setColorAt(1.0, QColor(QStringLiteral("#0B4F6C")));',
    'gradient.setColorAt(1.0, QColor(QStringLiteral("#2F2F2F")));',
)
text = text.replace(
    'painter.setBrush(QColor(QStringLiteral("#BFE8E3")));',
    'painter.setBrush(QColor(QStringLiteral("#D4D4D8")));',
)
text = text.replace(
    '    window.setStyleSheet(polishedStyleSheet());\n',
    '',
)
text = text.replace(
    'QColor color(QStringLiteral("#344054"));',
    'QColor color(QStringLiteral("#27272A"));',
)
text = text.replace(
    'color = QColor(QStringLiteral("#D8E5F2"));',
    'color = QColor(QStringLiteral("#F4F4F5"));',
)
p.write_text(text, encoding="utf-8")


# -----------------------------------------------------------------------------
# Regenerate the embedded Windows .ico as a monochrome flask icon.
# Pure stdlib PNG-in-ICO writer; no Pillow dependency.
# -----------------------------------------------------------------------------
w = h = 64
pixels = bytearray(w * h * 4)


def set_px(x, y, rgba):
    if 0 <= x < w and 0 <= y < h:
        i = (y * w + x) * 4
        pixels[i:i+4] = bytes(rgba)


def inside_round_rect(x, y, x0, y0, x1, y1, r):
    if x0 + r <= x <= x1 - r or y0 + r <= y <= y1 - r:
        return x0 <= x <= x1 and y0 <= y <= y1
    cx = x0 + r if x < x0 + r else x1 - r
    cy = y0 + r if y < y0 + r else y1 - r
    return (x - cx) ** 2 + (y - cy) ** 2 <= r ** 2


def draw_disc(cx, cy, r, rgba):
    for y in range(cy-r, cy+r+1):
        for x in range(cx-r, cx+r+1):
            if (x-cx)**2 + (y-cy)**2 <= r*r:
                set_px(x, y, rgba)


def draw_line(x0, y0, x1, y1, width, rgba):
    dx = abs(x1-x0)
    sx = 1 if x0 < x1 else -1
    dy = -abs(y1-y0)
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        draw_disc(x0, y0, max(1, width//2), rgba)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


for y in range(h):
    for x in range(w):
        if inside_round_rect(x, y, 3, 3, 60, 60, 14):
            set_px(x, y, (17, 17, 17, 255))

white = (255, 255, 255, 255)
gray = (212, 212, 216, 255)
draw_line(25, 15, 39, 15, 3, white)
draw_line(27, 16, 27, 28, 3, white)
draw_line(37, 16, 37, 28, 3, white)
draw_line(27, 28, 17, 49, 3, white)
draw_line(37, 28, 47, 49, 3, white)
draw_line(17, 49, 47, 49, 3, white)
draw_line(21, 41, 43, 41, 2, gray)
draw_disc(27, 45, 2, gray)
draw_disc(35, 46, 2, gray)
draw_disc(39, 43, 1, gray)

raw = bytearray()
for y in range(h):
    raw.append(0)
    row = pixels[y*w*4:(y+1)*w*4]
    raw.extend(row)


def chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
png += chunk(b"IEND", b"")
ico = struct.pack("<HHH", 0, 1, 1)
ico += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(png), 22)
ico += png
Path("native/qt/resources/app.ico").write_bytes(ico)

print("Decision dashboard UI migration applied.")
