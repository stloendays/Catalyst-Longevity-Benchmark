#include "PetWindow.h"

#include "AppLogger.h"

#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
bool reminderUiChinese() {
    return QSettings().value(QStringLiteral("ui/language"), QStringLiteral("en"))
        .toString().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive);
}
}

void PetWindow::createQuickReminder() {
    const bool zh = reminderUiChinese();

    QDialog dialog(this);
    dialog.setWindowTitle(zh ? QStringLiteral("Tony 快速提醒")
                             : QStringLiteral("Tony Quick Reminder"));
    dialog.setMinimumWidth(420);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    form->setSpacing(10);

    auto *minutesInput = new QSpinBox(&dialog);
    minutesInput->setRange(1, 7 * 24 * 60);
    minutesInput->setValue(20);
    minutesInput->setSuffix(zh ? QStringLiteral(" 分钟") : QStringLiteral(" min"));

    auto *textInput = new QLineEdit(&dialog);
    textInput->setPlaceholderText(
        zh ? QStringLiteral("例如：起来活动一下")
           : QStringLiteral("e.g. stand up and stretch"));
    textInput->setClearButtonEnabled(true);

    form->addRow(
        zh ? QStringLiteral("多久后提醒") : QStringLiteral("Remind me in"),
        minutesInput);
    form->addRow(
        zh ? QStringLiteral("提醒内容") : QStringLiteral("Reminder"),
        textInput);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    auto *okButton = buttons->button(QDialogButtonBox::Ok);
    okButton->setText(zh ? QStringLiteral("设置提醒") : QStringLiteral("Set reminder"));
    okButton->setEnabled(false);
    buttons->button(QDialogButtonBox::Cancel)
        ->setText(zh ? QStringLiteral("取消") : QStringLiteral("Cancel"));
    layout->addWidget(buttons);

    connect(textInput, &QLineEdit::textChanged, &dialog, [okButton](const QString &text){
        okButton->setEnabled(!text.trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    textInput->setFocus();
    if(dialog.exec() != QDialog::Accepted) return;

    const int minutes = minutesInput->value();
    const QString text = textInput->text().trimmed();
    if(text.isEmpty()) return;

    const QDateTime dueUtc =
        QDateTime::currentDateTimeUtc().addSecs(static_cast<qint64>(minutes) * 60);
    const QString title = zh ? QStringLiteral("Tony 提醒") : QStringLiteral("Tony Reminder");
    const QString id = localBridge_.createLocalReminder(title, text.left(500), dueUtc);
    if(id.isEmpty()) return;

    const QString localTime = dueUtc.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    tray_.showMessage(
        title,
        zh ? QStringLiteral("已设置：%1").arg(localTime)
           : QStringLiteral("Set for %1").arg(localTime),
        QSystemTrayIcon::Information,
        4200);
    AppLogger::recordOperatorEvent(
        QStringLiteral("reminder_created_local_ui"),
        {},
        QJsonObject{{QStringLiteral("delay_minutes"), minutes}});
}
