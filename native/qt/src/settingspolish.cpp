#include <QApplication>
#include <QCoreApplication>
#include <QFrame>
#include <QLabel>
#include <QLayoutItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QLabel* detailLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("mutedText"));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QFrame* informationBlock(const QString& title, const QString& body, QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("infoPanel"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel(title, frame);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setWordWrap(true);
    titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(titleLabel);
    layout->addWidget(detailLabel(body, frame));
    return frame;
}

void polishSettingsPage(QWidget* root) {
    if (!root) return;

    QLabel* productHeading = nullptr;
    const auto labels = root->findChildren<QLabel*>();
    for (auto* label : labels) {
        if (label->objectName() == QStringLiteral("pageHeading")
            && label->text() == QStringLiteral("设置")) {
            label->setText(QStringLiteral("设置与软件信息"));
            label->setWordWrap(true);
        }
        if (label->objectName() == QStringLiteral("pageHeading")
            && label->text() == QStringLiteral("智策")) {
            productHeading = label;
        }
    }

    if (!productHeading) return;
    auto* card = qobject_cast<QFrame*>(productHeading->parentWidget());
    auto* layout = card ? qobject_cast<QVBoxLayout*>(card->layout()) : nullptr;
    if (!card || !layout || card->property("zhiceSettingsPolished").toBool()) return;
    card->setProperty("zhiceSettingsPolished", true);

    layout->setSpacing(8);
    productHeading->setText(QStringLiteral("智策 · V1.0"));
    productHeading->setWordWrap(true);
    productHeading->setTextInteractionFlags(Qt::TextSelectableByMouse);

    const auto directLabels = card->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* label : directLabels) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        const QString text = label->text();
        if (text == QStringLiteral("催化剂长期稳定性评估与实验决策")) {
            label->setText(QStringLiteral("软件全称：催化剂长期稳定性评估与实验决策系统"));
        } else if (text == QStringLiteral("技术架构：C++20 · Qt 6 · SQLite。")) {
            label->setText(QStringLiteral("版本与平台：V1.0 · Windows 10/11 x64 · C++20 · Qt 6 · SQLite。"));
        } else if (text == QStringLiteral("数据输入：CSV / Excel .xlsx。")) {
            label->setText(QStringLiteral("数据与资料输入：CSV、Excel .xlsx；PDF、TXT、Markdown、CSV、TSV。"));
        } else if (text == QStringLiteral("项目存储：本地 .clrproj 文件（实验数据、资料关联和确认状态）。")) {
            label->setText(QStringLiteral("项目存储：本地 .clrproj（SQLite）文件，保存实验数据、资料关联与人工确认状态。"));
        } else if (text == QStringLiteral("核心功能：数据检查、寿命分析、同时间对比、实验建议、资料整理和 PDF 报告。")) {
            label->setText(QStringLiteral("核心功能：数据质量检查、寿命分析、条件可比性检查、同时间对比、实验建议、资料管理、公开参考匹配和 PDF 报告。"));
        }
    }

    if (layout->count() > 0) {
        auto* lastItem = layout->itemAt(layout->count() - 1);
        if (lastItem && lastItem->spacerItem()) {
            delete layout->takeAt(layout->count() - 1);
        }
    }

    layout->addWidget(informationBlock(
        QStringLiteral("运行方式与默认状态"),
        QStringLiteral(
            "核心分析、项目保存、5 组内置演示数据和公开参考库均可在本地使用。"
            "未导入实验数据时，软件会显示“未载入数据”“尚未分析”或“待分析”等明确状态，"
            "不会用空白区域表示未知状态。"),
        card));

    layout->addWidget(informationBlock(
        QStringLiteral("资料、AI 与接口边界"),
        QStringLiteral(
            "AI 助手只接收已经人工确认并关联到明确催化剂和时间点的资料；外部 AI 不是核心分析的必需依赖。"
            "自驱动实验室接口默认关闭，硬件执行默认禁用，避免未授权的外部控制。"),
        card));

    layout->addWidget(informationBlock(
        QStringLiteral("软件著作权登记信息"),
        QStringLiteral(
            "登记建议全称：催化剂长期稳定性评估与实验决策系统；建议简称：催化剂稳定性决策系统；"
            "当前软件版本：V1.0。登记材料以当前 C++ / Qt Windows 桌面程序已经实现的功能为准。"),
        card));

    auto* footer = detailLabel(
        QStringLiteral("软件信息为固定可读内容，不依赖当前项目是否已载入数据。"), card);
    footer->setObjectName(QStringLiteral("decisionDetail"));
    layout->addWidget(footer);
}

void installSettingsPolish() {
    QTimer::singleShot(0, []() {
        const auto windows = QApplication::topLevelWidgets();
        for (auto* window : windows) polishSettingsPage(window);
    });
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(installSettingsPolish)
