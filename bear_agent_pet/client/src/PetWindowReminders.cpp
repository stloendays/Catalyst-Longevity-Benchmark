#include "PetWindow.h"

#include "AppLogger.h"

#include <QDateTime>
#include <QInputDialog>
#include <QJsonObject>
#include <QLineEdit>
#include <QSettings>

namespace {
bool reminderUiChinese() {
    return QSettings().value(QStringLiteral("ui/language"), QStringLiteral("en"))
        .toString().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive);
}
}

void PetWindow::createQuickReminder() {
    const bool zh = reminderUiChinese();
    bool accepted = false;
    const int minutes = QInputDialog::getInt(
        this,
        zh ? QStringLiteral("Tony 快速提醒") : QStringLiteral("Tony Quick Reminder"),
        zh ? QStringLiteral("多少分钟后提醒？") : QStringLiteral("Remind me in how many minutes?"),
        20,
        1,
        7 * 24 * 60,
        1,
        &accepted);
    if(!accepted) return;

    const QString text = QInputDialog::getText(
        this,
        zh ? QStringLiteral("提醒内容") : QStringLiteral("Reminder text"),
        zh ? QStringLiteral("Tony 到时要提醒你什么？") : QStringLiteral("What should Tony remind you about?"),
        QLineEdit::Normal,
        {},
        &accepted).trimmed();
    if(!accepted || text.isEmpty()) return;

    const QDateTime dueUtc = QDateTime::currentDateTimeUtc().addSecs(static_cast<qint64>(minutes) * 60);
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
