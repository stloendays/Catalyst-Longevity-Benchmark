#include "analysisengine.h"
#include "builtindatasets.h"
#include "csvreader.h"
#include "documentanalyzer.h"
#include "evidencepacket.h"
#include "integrationgateway.h"
#include "mainwindow.h"
#include "projectstore.h"
#include "reportexporter.h"
#include "referenceknowledge.h"
#include "researchadvisor.h"

#include "xlsxdocument.h"

#include <QApplication>
#include <QColor>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPageSize>
#include <QPdfWriter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QStatusBar>
#include <QStringList>
#include <QTemporaryDir>

namespace {

enum class UiIcon {
    Project,
    Dashboard,
    Import,
    Document,
    Analysis,
    Ai,
    Settings,
    NewProject,
    Open,
    Save,
    SaveAs,
    Demo,
    Refresh,
    Export,
    Link,
    Unlink,
    Check,
    Undo
};

QIcon makeUiIcon(UiIcon type, const QColor& color) {
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(40.0 / 24.0, 40.0 / 24.0);

    QPen pen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const auto rounded = [&painter](double x, double y, double w, double h, double r = 2.0) {
        painter.drawRoundedRect(QRectF(x, y, w, h), r, r);
    };

    switch (type) {
    case UiIcon::Project:
        painter.drawPath([] {
            QPainterPath p;
            p.moveTo(3.5, 7.5);
            p.lineTo(9.0, 7.5);
            p.lineTo(10.8, 5.2);
            p.lineTo(20.5, 5.2);
            p.lineTo(20.5, 18.8);
            p.lineTo(3.5, 18.8);
            p.closeSubpath();
            return p;
        }());
        painter.drawLine(QPointF(3.8, 9.6), QPointF(20.2, 9.6));
        break;
    case UiIcon::Dashboard:
        rounded(3.5, 3.5, 7.0, 7.0, 1.4);
        rounded(13.5, 3.5, 7.0, 5.0, 1.4);
        rounded(3.5, 13.5, 7.0, 7.0, 1.4);
        rounded(13.5, 11.5, 7.0, 9.0, 1.4);
        break;
    case UiIcon::Import:
        painter.drawLine(QPointF(12, 3.5), QPointF(12, 14.2));
        painter.drawLine(QPointF(8.3, 10.5), QPointF(12, 14.2));
        painter.drawLine(QPointF(15.7, 10.5), QPointF(12, 14.2));
        painter.drawPath([] {
            QPainterPath p;
            p.moveTo(4.0, 15.3);
            p.lineTo(4.0, 19.7);
            p.lineTo(20.0, 19.7);
            p.lineTo(20.0, 15.3);
            return p;
        }());
        break;
    case UiIcon::Document:
        rounded(5.0, 2.8, 13.8, 18.4, 1.8);
        painter.drawLine(QPointF(8.2, 8.0), QPointF(15.8, 8.0));
        painter.drawLine(QPointF(8.2, 11.5), QPointF(15.8, 11.5));
        painter.drawLine(QPointF(8.2, 15.0), QPointF(13.9, 15.0));
        break;
    case UiIcon::Analysis: {
        painter.drawLine(QPointF(4.0, 19.5), QPointF(4.0, 4.3));
        painter.drawLine(QPointF(4.0, 19.5), QPointF(20.0, 19.5));
        QPainterPath path;
        path.moveTo(6.2, 15.6);
        path.lineTo(10.0, 11.9);
        path.lineTo(13.1, 13.4);
        path.lineTo(18.2, 7.2);
        painter.drawPath(path);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(6.2, 15.6), 1.0, 1.0);
        painter.drawEllipse(QPointF(10.0, 11.9), 1.0, 1.0);
        painter.drawEllipse(QPointF(13.1, 13.4), 1.0, 1.0);
        painter.drawEllipse(QPointF(18.2, 7.2), 1.0, 1.0);
        break;
    }
    case UiIcon::Ai:
        painter.drawEllipse(QPointF(8.0, 8.0), 2.6, 2.6);
        painter.drawEllipse(QPointF(16.2, 7.0), 2.2, 2.2);
        painter.drawEllipse(QPointF(12.2, 16.0), 2.8, 2.8);
        painter.drawLine(QPointF(10.4, 9.3), QPointF(14.2, 8.1));
        painter.drawLine(QPointF(9.6, 10.0), QPointF(11.2, 13.5));
        painter.drawLine(QPointF(15.2, 9.0), QPointF(13.5, 13.5));
        painter.drawLine(QPointF(19.0, 14.7), QPointF(19.0, 19.7));
        painter.drawLine(QPointF(16.5, 17.2), QPointF(21.5, 17.2));
        break;
    case UiIcon::Settings:
        painter.drawEllipse(QPointF(12.0, 12.0), 4.0, 4.0);
        painter.drawEllipse(QPointF(12.0, 12.0), 1.3, 1.3);
        painter.drawLine(QPointF(12.0, 3.1), QPointF(12.0, 6.1));
        painter.drawLine(QPointF(12.0, 17.9), QPointF(12.0, 20.9));
        painter.drawLine(QPointF(3.1, 12.0), QPointF(6.1, 12.0));
        painter.drawLine(QPointF(17.9, 12.0), QPointF(20.9, 12.0));
        painter.drawLine(QPointF(5.7, 5.7), QPointF(7.8, 7.8));
        painter.drawLine(QPointF(16.2, 16.2), QPointF(18.3, 18.3));
        painter.drawLine(QPointF(18.3, 5.7), QPointF(16.2, 7.8));
        painter.drawLine(QPointF(7.8, 16.2), QPointF(5.7, 18.3));
        break;
    case UiIcon::NewProject:
        rounded(5.0, 3.0, 12.5, 18.0, 1.6);
        painter.drawLine(QPointF(12.2, 10.0), QPointF(12.2, 16.0));
        painter.drawLine(QPointF(9.2, 13.0), QPointF(15.2, 13.0));
        break;
    case UiIcon::Open:
        painter.drawPath([] {
            QPainterPath p;
            p.moveTo(3.5, 8.0);
            p.lineTo(9.0, 8.0);
            p.lineTo(10.5, 5.6);
            p.lineTo(20.0, 5.6);
            p.lineTo(20.0, 9.0);
            p.lineTo(6.0, 9.0);
            p.lineTo(3.5, 18.8);
            p.closeSubpath();
            return p;
        }());
        painter.drawLine(QPointF(6.0, 9.0), QPointF(21.0, 9.0));
        painter.drawLine(QPointF(21.0, 9.0), QPointF(18.2, 18.8));
        painter.drawLine(QPointF(18.2, 18.8), QPointF(3.5, 18.8));
        break;
    case UiIcon::Save:
        rounded(4.0, 3.5, 16.0, 17.0, 1.8);
        rounded(7.0, 4.8, 9.5, 5.0, 0.8);
        rounded(7.0, 13.0, 10.0, 5.5, 1.0);
        break;
    case UiIcon::SaveAs:
        rounded(3.5, 3.5, 14.5, 17.0, 1.8);
        rounded(6.0, 4.8, 8.6, 4.7, 0.8);
        rounded(6.0, 12.6, 8.7, 5.3, 1.0);
        painter.drawLine(QPointF(15.7, 17.8), QPointF(20.8, 12.7));
        painter.drawLine(QPointF(19.0, 11.8), QPointF(21.7, 14.5));
        break;
    case UiIcon::Demo:
        painter.drawEllipse(QPointF(12.0, 12.0), 8.3, 8.3);
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF{QPointF(10.4, 8.5), QPointF(16.0, 12.0), QPointF(10.4, 15.5)});
        break;
    case UiIcon::Refresh:
        painter.drawArc(QRectF(4.2, 4.2, 15.6, 15.6), 35 * 16, 265 * 16);
        painter.drawLine(QPointF(18.9, 5.2), QPointF(19.7, 9.2));
        painter.drawLine(QPointF(19.7, 9.2), QPointF(15.7, 8.3));
        break;
    case UiIcon::Export:
        rounded(4.0, 9.0, 16.0, 11.0, 1.8);
        painter.drawLine(QPointF(12.0, 15.0), QPointF(12.0, 3.8));
        painter.drawLine(QPointF(8.5, 7.2), QPointF(12.0, 3.8));
        painter.drawLine(QPointF(15.5, 7.2), QPointF(12.0, 3.8));
        break;
    case UiIcon::Link:
        painter.drawRoundedRect(QRectF(3.2, 8.3, 9.5, 7.2), 3.6, 3.6);
        painter.drawRoundedRect(QRectF(11.3, 8.3, 9.5, 7.2), 3.6, 3.6);
        painter.drawLine(QPointF(8.9, 11.9), QPointF(15.1, 11.9));
        break;
    case UiIcon::Unlink:
        painter.drawRoundedRect(QRectF(3.2, 8.3, 8.6, 7.2), 3.6, 3.6);
        painter.drawRoundedRect(QRectF(12.2, 8.3, 8.6, 7.2), 3.6, 3.6);
        painter.drawLine(QPointF(4.7, 19.5), QPointF(19.3, 4.5));
        break;
    case UiIcon::Check:
        painter.drawEllipse(QPointF(12.0, 12.0), 8.3, 8.3);
        painter.drawLine(QPointF(8.0, 12.2), QPointF(10.8, 15.0));
        painter.drawLine(QPointF(10.8, 15.0), QPointF(16.5, 9.2));
        break;
    case UiIcon::Undo:
        painter.drawArc(QRectF(5.0, 5.0, 14.0, 14.0), 25 * 16, 250 * 16);
        painter.drawLine(QPointF(6.1, 5.5), QPointF(5.1, 9.5));
        painter.drawLine(QPointF(5.1, 9.5), QPointF(9.2, 8.7));
        break;
    }

    painter.end();
    return QIcon(pixmap);
}

QIcon makeApplicationIcon() {
    QPixmap pixmap(128, 128);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient gradient(10, 8, 118, 120);
    gradient.setColorAt(0.0, QColor(QStringLiteral("#111111")));
    gradient.setColorAt(1.0, QColor(QStringLiteral("#3F3F46")));
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(QRectF(6, 6, 116, 116), 27, 27);

    QPen white(QColor(QStringLiteral("#FFFFFF")), 6.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(white);
    painter.setBrush(Qt::NoBrush);

    QPainterPath flask;
    flask.moveTo(47, 27);
    flask.lineTo(47, 53);
    flask.lineTo(27, 91);
    flask.quadTo(23, 100, 33, 104);
    flask.lineTo(95, 104);
    flask.quadTo(105, 100, 101, 91);
    flask.lineTo(81, 53);
    flask.lineTo(81, 27);
    painter.drawPath(flask);
    painter.drawLine(QPointF(43, 27), QPointF(85, 27));
    painter.drawLine(QPointF(40, 79), QPointF(88, 79));

    painter.setBrush(QColor(QStringLiteral("#D4D4D8")));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(52, 88), 5.0, 5.0);
    painter.drawEllipse(QPointF(70, 91), 3.8, 3.8);
    painter.drawEllipse(QPointF(79, 84), 3.0, 3.0);
    painter.end();

    return QIcon(pixmap);
}

QString polishedStyleSheet() {
    return QStringLiteral(R"(
        QMainWindow, QWidget {
            background: #F7F9FC;
            color: #182230;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
        }
        QMainWindow { background: #F7F9FC; }
        QStatusBar {
            background: #FFFFFF;
            color: #64748B;
            border-top: 1px solid #E7ECF2;
            min-height: 30px;
        }
        QStatusBar QLabel { color: #64748B; padding: 0 8px; }

        #sidebar {
            background: #0B1220;
            border: none;
        }
        #brand {
            color: #F8FAFC;
            background: #111C30;
            border: 1px solid #20304A;
            border-radius: 14px;
            padding: 14px 15px;
            font-size: 16px;
            font-weight: 700;
            letter-spacing: 0.4px;
            line-height: 1.45;
        }
        #sidebarFoot {
            color: #8090A6;
            font-size: 11px;
            line-height: 1.5;
            padding: 8px 4px 2px 4px;
        }
        #navButton {
            color: #C9D5E4;
            background: transparent;
            border: 1px solid transparent;
            border-radius: 10px;
            padding: 10px 13px;
            text-align: left;
            font-weight: 500;
            min-height: 24px;
        }
        #navButton:hover {
            background: #121E32;
            border-color: #20304A;
            color: #FFFFFF;
        }
        #navButton:checked {
            background: #123E3B;
            border-color: #1B6E67;
            color: #F1FFFC;
            font-weight: 650;
        }
        #navButton:pressed { background: #0E3431; }

        #pageHeading {
            color: #0F172A;
            font-size: 23px;
            font-weight: 700;
        }
        #mutedText {
            color: #667085;
            line-height: 1.55;
        }
        #metricCard, #panel {
            background: #FFFFFF;
            border: 1px solid #E4EAF1;
            border-radius: 13px;
        }
        #metricCard { min-height: 66px; }
        #infoPanel {
            background: #F0F9F7;
            border: 1px solid #C7E9E4;
            border-radius: 12px;
        }
        #metricTitle {
            color: #758195;
            font-size: 12px;
            font-weight: 500;
        }
        #metricValue {
            color: #0F172A;
            font-size: 20px;
            font-weight: 700;
        }
        #sectionTitle {
            color: #172033;
            font-size: 15px;
            font-weight: 700;
        }
        #sourcePath {
            color: #0F766E;
            font-weight: 600;
        }
        #guardStatus {
            color: #0F766E;
            font-size: 14px;
            font-weight: 700;
            padding: 4px 0;
        }

        #primaryButton {
            background: #0F766E;
            color: #FFFFFF;
            border: 1px solid #0F766E;
            border-radius: 9px;
            padding: 9px 16px;
            font-weight: 600;
            min-height: 22px;
        }
        #primaryButton:hover {
            background: #0B675F;
            border-color: #0B675F;
        }
        #primaryButton:pressed {
            background: #085A53;
            border-color: #085A53;
        }
        #primaryButton:disabled {
            background: #A9C9C5;
            border-color: #A9C9C5;
            color: #F5FAF9;
        }
        #secondaryButton {
            background: #FFFFFF;
            color: #344054;
            border: 1px solid #D7DEE8;
            border-radius: 9px;
            padding: 9px 16px;
            font-weight: 600;
            min-height: 22px;
        }
        #secondaryButton:hover {
            background: #F8FAFC;
            border-color: #B8C3D1;
            color: #0F172A;
        }
        #secondaryButton:pressed { background: #F1F5F9; }

        QLineEdit, QComboBox, QDoubleSpinBox, QTextEdit {
            background: #FFFFFF;
            color: #172033;
            border: 1px solid #D8E0EA;
            border-radius: 8px;
            padding: 7px 9px;
            selection-background-color: #CDEBE7;
            selection-color: #0F172A;
        }
        QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QTextEdit:focus {
            border: 1px solid #2A8C83;
            background: #FFFFFF;
        }
        QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QTextEdit:disabled {
            background: #F1F4F8;
            color: #98A2B3;
        }
        QComboBox::drop-down, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            border: none;
            background: transparent;
        }

        QTableWidget {
            background: #FFFFFF;
            alternate-background-color: #FAFBFD;
            border: 1px solid #E3E9F0;
            border-radius: 10px;
            gridline-color: #EEF2F6;
            selection-background-color: #DDF2EF;
            selection-color: #172033;
        }
        QHeaderView::section {
            background: #F5F7FA;
            color: #475467;
            border: none;
            border-right: 1px solid #E9EDF3;
            border-bottom: 1px solid #E3E9F0;
            padding: 9px 8px;
            font-weight: 650;
        }
        QTableCornerButton::section {
            background: #F5F7FA;
            border: none;
            border-bottom: 1px solid #E3E9F0;
        }
        QTableWidget::item {
            padding: 7px;
            border: none;
        }
        QTableWidget::item:selected {
            background: #DDF2EF;
            color: #172033;
        }

        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 3px 2px 3px 2px;
        }
        QScrollBar::handle:vertical {
            background: #CBD5E1;
            border-radius: 4px;
            min-height: 28px;
        }
        QScrollBar::handle:vertical:hover { background: #AEB9C8; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal {
            background: transparent;
            height: 10px;
            margin: 2px 3px 2px 3px;
        }
        QScrollBar::handle:horizontal {
            background: #CBD5E1;
            border-radius: 4px;
            min-width: 28px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

        QToolTip {
            background: #172033;
            color: #FFFFFF;
            border: 1px solid #29364A;
            border-radius: 6px;
            padding: 6px 8px;
        }
    )");
}

QString preferredUiFontFamily() {
    const QStringList families = QFontDatabase::families();
    const QStringList preferred = {
        QStringLiteral("DengXian"),
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei")
    };
    for (const auto& family : preferred) {
        if (families.contains(family, Qt::CaseInsensitive)) return family;
    }
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
}

void polishChineseCopy(catalyst::MainWindow& window) {
    const auto labels = window.findChildren<QLabel*>();
    for (auto* label : labels) {
        const QString text = label->text();
        if (text == QStringLiteral("Ready")) {
            label->setText(QStringLiteral("就绪"));
        } else if (text == QStringLiteral("CATALYST\nLONGEVITY")) {
            label->setText(QStringLiteral("催化剂寿命研究\nCATALYST LONGEVITY"));
        } else if (text == QStringLiteral("Native Desktop\nC++ / Qt 6")) {
            label->setText(QStringLiteral("原生桌面版\nC++20 · Qt 6"));
        } else if (text == QStringLiteral("Native AI integration roadmap")) {
            label->setText(QStringLiteral("AI 能力接入路线"));
        } else if (text == QStringLiteral("Evidence Packet  ·  HTTP/API 客户端  ·  AI Analyst  ·  Evidence Critic  ·  审计日志")) {
            label->setText(QStringLiteral("证据包 · API 客户端 · AI 分析器 · 证据审查器 · 审计日志"));
        } else if (text == QStringLiteral("资料分析与 Evidence Packet")) {
            label->setText(QStringLiteral("资料证据解析与证据包"));
        } else if (text == QStringLiteral("Evidence Packet · AI 输入边界")) {
            label->setText(QStringLiteral("证据包 · AI 输入边界"));
        } else if (text == QStringLiteral(
                       "Packet 只包含已完成人工条件复核且绑定到明确催化剂与时间的证据；其他候选自动排除。这里展示的 Markdown 将作为后续 AI Analyst / Evidence Critic 的可审计上下文基础。")) {
            label->setText(QStringLiteral(
                "证据包只纳入已完成人工条件复核，并绑定到明确催化剂和时间点的条目；其余候选自动排除。这里展示的 Markdown 将作为后续 AI 分析器与证据审查器的可审计上下文。"));
        } else if (text == QStringLiteral(
                       "原生证据候选现在可以持久化、绑定并人工完成条件复核。下一阶段将把 Evidence Packet、AI Analyst、Evidence Critic 与外部数据库客户端接入这里。")) {
            label->setText(QStringLiteral(
                "证据候选可在本地项目中持久化，并完成催化剂、时间点绑定与人工条件复核。后续 AI 分析器、证据审查器和外部数据库客户端将统一接入此工作区。"));
        }
    }

    const auto buttons = window.findChildren<QPushButton*>();
    for (auto* button : buttons) {
        const QString text = button->text();
        if (text == QStringLiteral("资料分析")) {
            button->setText(QStringLiteral("资料"));
        } else if (text == QStringLiteral("寿命分析")) {
            button->setText(QStringLiteral("分析"));
        } else if (text == QStringLiteral("AI 工作区")) {
            button->setText(QStringLiteral("AI 助手"));
        } else if (text == QStringLiteral("载入示例")) {
            button->setText(QStringLiteral("示例数据"));
        } else if (text == QStringLiteral("选择 CSV / Excel")) {
            button->setText(QStringLiteral("选择数据文件"));
        } else if (text == QStringLiteral("选择 PDF / 资料文件")) {
            button->setText(QStringLiteral("选择资料文件"));
        } else if (text == QStringLiteral("绑定")) {
            button->setText(QStringLiteral("关联"));
        } else if (text == QStringLiteral("解除")) {
            button->setText(QStringLiteral("取消关联"));
        } else if (text == QStringLiteral("条件已复核（仅上下文）")) {
            button->setText(QStringLiteral("确认"));
        } else if (text == QStringLiteral("退回待复核")) {
            button->setText(QStringLiteral("取消确认"));
        }
    }
}

bool iconForButton(const QString& text, UiIcon* icon, QString* tooltip) {
    if (text == QStringLiteral("项目")) {
        *icon = UiIcon::Project;
        *tooltip = QStringLiteral("项目文件与保存管理");
    } else if (text == QStringLiteral("总览") || text == QStringLiteral("首页")) {
        *icon = UiIcon::Dashboard;
        *tooltip = QStringLiteral("查看首页概况");
    } else if (text == QStringLiteral("数据导入") || text == QStringLiteral("数据")) {
        *icon = UiIcon::Import;
        *tooltip = QStringLiteral("导入 CSV 或 Excel 实验数据");
    } else if (text == QStringLiteral("资料证据") || text == QStringLiteral("资料")) {
        *icon = UiIcon::Document;
        *tooltip = QStringLiteral("导入并整理论文资料");
    } else if (text == QStringLiteral("寿命与条件") || text == QStringLiteral("分析")) {
        *icon = UiIcon::Analysis;
        *tooltip = QStringLiteral("查看寿命指标和条件检查");
    } else if (text == QStringLiteral("AI 智能研判") || text == QStringLiteral("AI 助手")) {
        *icon = UiIcon::Ai;
        *tooltip = QStringLiteral("使用 AI 助手分析已确认资料");
    } else if (text == QStringLiteral("设置")) {
        *icon = UiIcon::Settings;
        *tooltip = QStringLiteral("软件信息与运行设置");
    } else if (text == QStringLiteral("新建项目")) {
        *icon = UiIcon::NewProject;
        *tooltip = QStringLiteral("创建新的空白项目");
    } else if (text == QStringLiteral("打开项目")) {
        *icon = UiIcon::Open;
        *tooltip = QStringLiteral("打开已有 .clrproj 项目");
    } else if (text == QStringLiteral("保存")) {
        *icon = UiIcon::Save;
        *tooltip = QStringLiteral("保存当前项目");
    } else if (text == QStringLiteral("另存为")) {
        *icon = UiIcon::SaveAs;
        *tooltip = QStringLiteral("将当前项目保存为新文件");
    } else if (text == QStringLiteral("载入示例数据") || text == QStringLiteral("示例数据") || text == QStringLiteral("内置数据集")) {
        *icon = UiIcon::Demo;
        *tooltip = QStringLiteral("选择内置数据集进行分析或功能演示");
    } else if (text == QStringLiteral("导入数据") || text == QStringLiteral("选择数据文件")) {
        *icon = UiIcon::Import;
        *tooltip = QStringLiteral("选择 CSV 或 Excel 数据文件");
    } else if (text == QStringLiteral("重新分析")) {
        *icon = UiIcon::Refresh;
        *tooltip = QStringLiteral("根据当前数据重新计算分析结果");
    } else if (text == QStringLiteral("导出 PDF")) {
        *icon = UiIcon::Export;
        *tooltip = QStringLiteral("导出当前分析 PDF 报告");
    } else if (text == QStringLiteral("选择资料文件")) {
        *icon = UiIcon::Document;
        *tooltip = QStringLiteral("选择 PDF、TXT、Markdown、CSV 或 TSV 资料");
    } else if (text == QStringLiteral("绑定证据") || text == QStringLiteral("关联")) {
        *icon = UiIcon::Link;
        *tooltip = QStringLiteral("将选中内容关联到催化剂和时间");
    } else if (text == QStringLiteral("解除绑定") || text == QStringLiteral("取消关联")) {
        *icon = UiIcon::Unlink;
        *tooltip = QStringLiteral("取消当前资料关联");
    } else if (text == QStringLiteral("完成条件复核") || text == QStringLiteral("确认")) {
        *icon = UiIcon::Check;
        *tooltip = QStringLiteral("确认已核对这条资料的关键实验条件");
    } else if (text == QStringLiteral("退回复核") || text == QStringLiteral("取消确认")) {
        *icon = UiIcon::Undo;
        *tooltip = QStringLiteral("取消确认，重新检查这条资料");
    } else {
        return false;
    }
    return true;
}

void applyWindowPolish(catalyst::MainWindow& window) {
    window.setWindowTitle(QStringLiteral("智策"));
    window.statusBar()->setSizeGripEnabled(false);

    if (auto* sidebar = window.findChild<QFrame*>(QStringLiteral("sidebar"))) {
        sidebar->setFixedWidth(258);
    }

    polishChineseCopy(window);

    const auto buttons = window.findChildren<QPushButton*>();
    for (auto* button : buttons) {
        button->setCursor(Qt::PointingHandCursor);
        UiIcon icon = UiIcon::Dashboard;
        QString tooltip;
        if (!iconForButton(button->text(), &icon, &tooltip)) continue;

        QColor color(QStringLiteral("#3F3F46"));
        if (button->objectName() == QStringLiteral("navButton")) {
            color = QColor(QStringLiteral("#4B5563"));
            button->setIconSize(QSize(19, 19));
        } else if (button->objectName() == QStringLiteral("primaryButton")) {
            color = QColor(QStringLiteral("#FFFFFF"));
            button->setIconSize(QSize(18, 18));
        } else {
            button->setIconSize(QSize(18, 18));
        }
        button->setIcon(makeUiIcon(icon, color));
        button->setToolTip(tooltip);
    }

}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Catalyst Longevity Research"));
    app.setApplicationName(QStringLiteral("智策"));
    app.setApplicationDisplayName(QStringLiteral("智策"));
    app.setStyle(QStringLiteral("Fusion"));
    app.setFont(QFont(preferredUiFontFamily(), 10));
    app.setWindowIcon(makeApplicationIcon());

    if (app.arguments().contains(QStringLiteral("--self-test"))) {
        const auto builtInDatasets = catalyst::BuiltInDatasets::all();
        if (builtInDatasets.size() != 5) return 30;
        for (const auto& dataset : builtInDatasets) {
            if (dataset.name.trimmed().isEmpty() || dataset.records.isEmpty()) return 31;
        }
        const auto referenceEntries = catalyst::ReferenceKnowledgeBase::entries();
        if (referenceEntries.size() < 7) return 32;
        const auto referenceMatches = catalyst::ReferenceKnowledgeBase::matchExperimentContext(builtInDatasets.front().records);
        if (referenceMatches.isEmpty() || referenceMatches.front().relevanceScore < 45) return 33;

        catalyst::IntegrationGateway selfTestGateway;
        catalyst::IntegrationGatewayConfig selfTestGatewayConfig;
        selfTestGatewayConfig.bindAddress = QStringLiteral("127.0.0.1");
        selfTestGatewayConfig.controlPort = 0;
        selfTestGatewayConfig.eventPort = 0;
        selfTestGatewayConfig.instrumentPort = 0;
        selfTestGatewayConfig.accessToken = QStringLiteral("self-test-token-0123456789abcdef");
        QString gatewayTestMessage;
        if (!selfTestGateway.start(selfTestGatewayConfig, &gatewayTestMessage)) return 35;
        if (!selfTestGateway.isRunning()
            || selfTestGateway.controlPort() == 0
            || selfTestGateway.eventPort() == 0
            || selfTestGateway.instrumentPort() == 0
            || selfTestGateway.accessToken() != selfTestGatewayConfig.accessToken) return 36;
        const auto gatewayCapabilities = selfTestGateway.capabilities();
        if (gatewayCapabilities.value(QStringLiteral("safety")).toObject()
                .value(QStringLiteral("hardware_actuation")).toBool(true)) return 37;
        selfTestGateway.stop();
        if (selfTestGateway.isRunning()) return 38;

        const auto records = catalyst::CsvReader::demoData();
        const auto result = catalyst::AnalysisEngine::analyze(records);
        if (result.totalObservations != 6 || result.catalysts.size() != 2) return 2;
        if (!result.latestSharedTimeHours.has_value() || result.latestSharedLeader.isEmpty()) return 3;

        auto mismatched = records;
        for (auto& record : mismatched) {
            if (record.catalyst == QStringLiteral("Catalyst B")) record.temperatureC = 750.0;
        }
        const auto mismatchResult = catalyst::AnalysisEngine::analyze(mismatched);
        if (!mismatchResult.conditionAudit.blocksDirectRanking()
            || !mismatchResult.latestSharedLeader.isEmpty()) return 4;

        QTemporaryDir tempDir;
        if (!tempDir.isValid()) return 5;

        const QString projectPath = tempDir.filePath(QStringLiteral("self-test.clrproj"));
        QString persistenceMessage;
        if (!catalyst::ProjectStore::saveProject(projectPath, records, &persistenceMessage)) return 6;
        QVector<catalyst::Record> loaded;
        if (!catalyst::ProjectStore::loadProject(projectPath, &loaded, &persistenceMessage)) return 7;
        if (loaded.size() != records.size()
            || loaded.front().catalyst != records.front().catalyst
            || loaded.front().temperatureC != records.front().temperatureC) return 8;

        const QString workbookPath = tempDir.filePath(QStringLiteral("self-test.xlsx"));
        {
            QXlsx::Document workbook;
            workbook.write(1, 1, QStringLiteral("催化剂"));
            workbook.write(1, 2, QStringLiteral("时间"));
            workbook.write(1, 3, QStringLiteral("性能"));
            workbook.write(1, 4, QStringLiteral("温度"));
            workbook.write(2, 1, QStringLiteral("Excel Catalyst"));
            workbook.write(2, 2, 0.0);
            workbook.write(2, 3, 81.0);
            workbook.write(2, 4, 650.0);
            workbook.write(3, 1, QStringLiteral("Excel Catalyst"));
            workbook.write(3, 2, 24.0);
            workbook.write(3, 3, 76.0);
            workbook.write(3, 4, 650.0);
            if (!workbook.saveAs(workbookPath)) return 9;
        }

        QString importMessage;
        const auto workbookRecords = catalyst::CsvReader::readFile(workbookPath, &importMessage);
        if (workbookRecords.size() != 2
            || workbookRecords.front().catalyst != QStringLiteral("Excel Catalyst")
            || !workbookRecords.front().temperatureC.has_value()
            || *workbookRecords.front().temperatureC != 650.0) return 10;

        const QString evidenceText = QStringLiteral(
            "DOI 10.1234/example.2026.42. Catalyst X was tested at 700 °C for 20 h. "
            "CH4 conversion remained 82%. Long-term stability was limited by coking and sintering, "
            "followed by regeneration.");
        const auto documentSignals = catalyst::DocumentAnalyzer::analyzeText(
            evidenceText, QStringLiteral("self-test.txt"));
        if (documentSignals.dois.isEmpty()
            || documentSignals.temperaturesC.isEmpty()
            || documentSignals.durationsHours.isEmpty()
            || documentSignals.ch4ConversionPercentCandidates.isEmpty()
            || !documentSignals.keywordEvidence.contains(QStringLiteral("stability"))
            || documentSignals.snippets.isEmpty()
            || documentSignals.sourceSha256.isEmpty()) return 11;

        auto evidenceItems = catalyst::DocumentAnalyzer::candidateItems(evidenceText, documentSignals);
        if (evidenceItems.isEmpty()) return 12;
        evidenceItems.front().sourcePage = 3;
        evidenceItems.front().boundCatalyst = QStringLiteral("Catalyst A");
        evidenceItems.front().boundTimeHours = 20.0;
        evidenceItems.front().status = QStringLiteral("condition_reviewed_context_only");
        evidenceItems.front().note = QStringLiteral(
            "self-test reviewed temperature, flow/space velocity, pressure, feed and metric semantics");

        const auto packet = catalyst::EvidencePacketBuilder::build(evidenceItems);
        if (!packet.readyForAi
            || packet.reviewedContextItems != 1
            || packet.contextItems.front().boundCatalyst != QStringLiteral("Catalyst A")
            || catalyst::EvidencePacketBuilder::toMarkdown(packet).isEmpty()) return 13;

        if (!catalyst::ProjectStore::saveProject(
                projectPath, records, evidenceItems, &persistenceMessage)) return 14;
        QVector<catalyst::Record> loadedWithEvidence;
        QVector<catalyst::EvidenceItem> loadedEvidence;
        if (!catalyst::ProjectStore::loadProject(
                projectPath, &loadedWithEvidence, &loadedEvidence, &persistenceMessage)) return 15;
        if (loadedWithEvidence.size() != records.size()
            || loadedEvidence.size() != evidenceItems.size()
            || loadedEvidence.front().sourceSha256 != evidenceItems.front().sourceSha256
            || loadedEvidence.front().sourcePage != 3
            || loadedEvidence.front().boundCatalyst != QStringLiteral("Catalyst A")
            || !loadedEvidence.front().boundTimeHours.has_value()
            || *loadedEvidence.front().boundTimeHours != 20.0
            || loadedEvidence.front().status != QStringLiteral("condition_reviewed_context_only")
            || loadedEvidence.front().note.isEmpty()) return 16;

        const QString inputPdfPath = tempDir.filePath(QStringLiteral("self-test-input.pdf"));
        {
            QPdfWriter writer(inputPdfPath);
            writer.setPageSize(QPageSize(QPageSize::A4));
            writer.setResolution(96);
            QPainter painter(&writer);
            if (!painter.isActive()) return 17;
            painter.setFont(QFont(QStringLiteral("Arial"), 13));
            painter.drawText(100, 150, QStringLiteral("DOI 10.5678/nativepdf.2026.1"));
            painter.drawText(100, 200, QStringLiteral("Catalyst PDF was tested at 680 deg C for 48 h."));
            painter.drawText(100, 250, QStringLiteral("CH4 conversion remained 77%. Stability decreased because of coking."));
            painter.end();
        }
        if (!QFileInfo::exists(inputPdfPath) || QFileInfo(inputPdfPath).size() <= 0) return 18;

        catalyst::DocumentReadResult pdfRead;
        QString pdfReadMessage;
        if (!catalyst::DocumentAnalyzer::readDocument(inputPdfPath, &pdfRead, &pdfReadMessage)) return 19;
        if (pdfRead.sourceFormat != QStringLiteral("pdf")
            || pdfRead.pageCount != 1
            || pdfRead.sourceSha256.isEmpty()
            || pdfRead.text.trimmed().isEmpty()) return 20;

        const auto pdfSignals = catalyst::DocumentAnalyzer::analyzeDocument(pdfRead);
        if (pdfSignals.pageCount != 1
            || pdfSignals.dois.isEmpty()
            || pdfSignals.temperaturesC.isEmpty()
            || pdfSignals.durationsHours.isEmpty()) return 21;
        const auto pdfEvidence = catalyst::DocumentAnalyzer::candidateItems(pdfRead.text, pdfSignals);
        if (pdfEvidence.isEmpty()) return 22;
        bool foundPageOne = false;
        for (const auto& item : pdfEvidence) {
            if (item.sourcePage == 1) {
                foundPageOne = true;
                break;
            }
        }
        if (!foundPageOne) return 23;

        const QString reportPath = tempDir.filePath(QStringLiteral("self-test-report.pdf"));
        QString reportMessage;
        if (!catalyst::ReportExporter::exportPdf(
                reportPath, records, result, QStringLiteral("self-test"), loadedEvidence, &reportMessage)) return 24;
        if (!QFileInfo::exists(reportPath) || QFileInfo(reportPath).size() <= 0) return 25;

        const auto dataCheck = catalyst::ResearchAdvisor::checkData(records, result);
        if (dataCheck.score <= 0 || dataCheck.items.isEmpty()) return 26;
        const auto experimentAdvice = catalyst::ResearchAdvisor::experimentAdvice(records, result);
        if (experimentAdvice.isEmpty()) return 27;
        bool hasExecutableAdvice = false;
        for (const auto& item : experimentAdvice) {
            if (item.feasibilityScore > 0 && !item.feasibility.isEmpty() && !item.basis.isEmpty()) {
                hasExecutableAdvice = true;
                break;
            }
        }
        if (!hasExecutableAdvice) return 34;
        const auto comparisons = catalyst::ResearchAdvisor::pairComparisons(records, result);
        if (comparisons.size() != 1
            || comparisons.front().catalystA.isEmpty()
            || comparisons.front().catalystB.isEmpty()
            || comparisons.front().sharedTimeHours <= 0.0) return 28;

        const auto mismatchComparisons = catalyst::ResearchAdvisor::pairComparisons(mismatched, mismatchResult);
        if (mismatchComparisons.size() != 1
            || mismatchComparisons.front().comparable
            || mismatchComparisons.front().status != QStringLiteral("条件不一致")) return 29;

        return 0;
    }

    catalyst::IntegrationGateway integrationGateway;
    const auto integrationConfig = catalyst::IntegrationGateway::configFromArguments(app.arguments());
    QString integrationMessage;
    const bool integrationStarted = !integrationConfig.enabled
        || integrationGateway.start(integrationConfig, &integrationMessage);

    catalyst::MainWindow window;
    applyWindowPolish(window);
    if (!integrationConfig.enabled) {
        window.statusBar()->showMessage(QStringLiteral("自驱动实验室接口已禁用。"), 8000);
    } else if (!integrationStarted) {
        window.statusBar()->showMessage(QStringLiteral("自驱动实验室接口未启动：%1").arg(integrationMessage), 15000);
    } else {
        window.statusBar()->showMessage(integrationMessage, 8000);
    }
    window.show();
    const int exitCode = app.exec();
    integrationGateway.stop();
    return exitCode;
}
