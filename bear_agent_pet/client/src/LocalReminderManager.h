#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

class LocalReminderManager : public QObject {
    Q_OBJECT
public:
    explicit LocalReminderManager(QObject *parent=nullptr);

    QString createReminder(const QString &title, const QString &text, const QDateTime &dueUtc);
    QJsonArray reminders() const;
    bool cancelReminder(const QString &id);
    int count() const;

signals:
    void reminderDue(const QString &id, const QString &title, const QString &text);

private:
    struct Reminder {
        QString id;
        QString title;
        QString text;
        QDateTime dueUtc;
    };

    void load();
    void save() const;
    void scheduleNext();
    void processDue();

    QList<Reminder> items_;
    QTimer timer_;
};
