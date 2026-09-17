#include "LocalReminderManager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>
#include <algorithm>
#include <limits>

namespace {
const auto kSettingsKey = QStringLiteral("tony/reminders/v1");
constexpr qint64 kMaxTimerDelayMs = 6LL * 60LL * 60LL * 1000LL;

QString normalizedTitle(QString value) {
    value = value.trimmed();
    return value.left(80);
}

QString normalizedText(QString value) {
    value = value.trimmed();
    return value.left(500);
}
}

LocalReminderManager::LocalReminderManager(QObject *parent)
    : QObject(parent) {
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, &LocalReminderManager::processDue);
    load();
    QTimer::singleShot(0, this, [this]{ processDue(); });
}

QString LocalReminderManager::createReminder(const QString &title,
                                             const QString &text,
                                             const QDateTime &dueUtc) {
    Reminder reminder;
    reminder.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    reminder.title = normalizedTitle(title);
    reminder.text = normalizedText(text);
    reminder.dueUtc = dueUtc.toUTC();
    items_.push_back(reminder);
    save();
    scheduleNext();
    return reminder.id;
}

QJsonArray LocalReminderManager::reminders() const {
    QList<Reminder> sorted = items_;
    std::sort(sorted.begin(), sorted.end(), [](const Reminder &a, const Reminder &b){
        return a.dueUtc < b.dueUtc;
    });

    QJsonArray out;
    for(const auto &reminder : sorted) {
        out.append(QJsonObject{
            {QStringLiteral("id"), reminder.id},
            {QStringLiteral("title"), reminder.title},
            {QStringLiteral("text"), reminder.text},
            {QStringLiteral("due_utc"), reminder.dueUtc.toString(Qt::ISODate)}
        });
    }
    return out;
}

bool LocalReminderManager::cancelReminder(const QString &id) {
    const QString needle = id.trimmed();
    for(int i = 0; i < items_.size(); ++i) {
        if(items_.at(i).id == needle) {
            items_.removeAt(i);
            save();
            scheduleNext();
            return true;
        }
    }
    return false;
}

int LocalReminderManager::count() const {
    return items_.size();
}

void LocalReminderManager::load() {
    items_.clear();
    const QByteArray raw = QSettings().value(kSettingsKey).toByteArray();
    if(raw.isEmpty()) return;

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if(!doc.isArray()) return;

    for(const auto &value : doc.array()) {
        if(!value.isObject()) continue;
        const auto obj = value.toObject();
        const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
        const QString title = normalizedTitle(obj.value(QStringLiteral("title")).toString());
        const QString text = normalizedText(obj.value(QStringLiteral("text")).toString());
        const QDateTime due = QDateTime::fromString(
            obj.value(QStringLiteral("due_utc")).toString(), Qt::ISODate).toUTC();
        if(id.isEmpty() || title.isEmpty() || !due.isValid()) continue;
        items_.push_back(Reminder{id, title, text, due});
    }
    scheduleNext();
}

void LocalReminderManager::save() const {
    QJsonArray array;
    for(const auto &reminder : items_) {
        array.append(QJsonObject{
            {QStringLiteral("id"), reminder.id},
            {QStringLiteral("title"), reminder.title},
            {QStringLiteral("text"), reminder.text},
            {QStringLiteral("due_utc"), reminder.dueUtc.toUTC().toString(Qt::ISODate)}
        });
    }
    QSettings().setValue(kSettingsKey, QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void LocalReminderManager::scheduleNext() {
    timer_.stop();
    if(items_.isEmpty()) return;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    qint64 delay = std::numeric_limits<qint64>::max();
    for(const auto &reminder : items_)
        delay = std::min(delay, now.msecsTo(reminder.dueUtc));

    if(delay <= 0) {
        timer_.start(50);
        return;
    }
    delay = std::min(delay, kMaxTimerDelayMs);
    timer_.start(static_cast<int>(std::min<qint64>(delay, std::numeric_limits<int>::max())));
}

void LocalReminderManager::processDue() {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QList<Reminder> due;
    for(int i = items_.size() - 1; i >= 0; --i) {
        if(items_.at(i).dueUtc <= now) {
            due.push_front(items_.at(i));
            items_.removeAt(i);
        }
    }

    if(!due.isEmpty()) save();
    for(const auto &reminder : due)
        emit reminderDue(reminder.id, reminder.title, reminder.text);
    scheduleNext();
}
