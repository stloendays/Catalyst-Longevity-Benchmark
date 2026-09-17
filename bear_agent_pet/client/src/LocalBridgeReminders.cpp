#include "LocalBridge.h"

QString LocalBridge::createLocalReminder(const QString &title, const QString &text, const QDateTime &dueUtc) {
    return reminders_.createReminder(title, text, dueUtc);
}

QJsonArray LocalBridge::localReminders() const {
    return reminders_.reminders();
}

bool LocalBridge::cancelLocalReminder(const QString &id) {
    return reminders_.cancelReminder(id);
}
