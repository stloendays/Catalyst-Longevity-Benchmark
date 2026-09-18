#include "TonyQuietMode.h"

#include "AppLogger.h"

#include <QJsonObject>
#include <QSettings>
#include <QtGlobal>

namespace {
const QString kQuietUntilKey = QStringLiteral("ux/quiet_until_utc_ms");
}

namespace TonyQuietMode {

QDateTime untilUtc() {
    const qint64 value = QSettings().value(kQuietUntilKey, 0).toLongLong();
    if(value <= 0) return {};
    return QDateTime::fromMSecsSinceEpoch(value, Qt::UTC);
}

bool isActive() {
    const QDateTime until = untilUtc();
    if(!until.isValid()) return false;
    if(QDateTime::currentDateTimeUtc() < until) return true;

    QSettings().remove(kQuietUntilKey);
    return false;
}

void clear() {
    QSettings().remove(kQuietUntilKey);
    AppLogger::recordOperatorEvent(QStringLiteral("quiet_mode_off"));
}

void setForMinutes(int minutes) {
    const int safeMinutes = qBound(1, minutes, 24 * 60);
    const QDateTime until = QDateTime::currentDateTimeUtc().addSecs(safeMinutes * 60);
    QSettings().setValue(kQuietUntilKey, until.toMSecsSinceEpoch());
    AppLogger::recordOperatorEvent(
        QStringLiteral("quiet_mode_on"),
        {},
        QJsonObject{{QStringLiteral("minutes"), safeMinutes}});
}

void setUntilTomorrowMorning(int hour) {
    const int safeHour = qBound(0, hour, 23);
    const QDateTime now = QDateTime::currentDateTime();
    QDate targetDate = now.date();
    QDateTime localTarget(targetDate, QTime(safeHour, 0));

    if(localTarget <= now)
        localTarget = QDateTime(targetDate.addDays(1), QTime(safeHour, 0));

    const QDateTime utcTarget = localTarget.toUTC();
    QSettings().setValue(kQuietUntilKey, utcTarget.toMSecsSinceEpoch());
    AppLogger::recordOperatorEvent(
        QStringLiteral("quiet_mode_on"),
        {},
        QJsonObject{
            {QStringLiteral("until_local"), localTarget.toString(Qt::ISODate)}
        });
}

QString remainingLabel(bool chinese) {
    const QDateTime until = untilUtc();
    if(!until.isValid() || QDateTime::currentDateTimeUtc() >= until)
        return chinese ? QStringLiteral("未开启") : QStringLiteral("Off");

    const qint64 seconds = QDateTime::currentDateTimeUtc().secsTo(until);
    const qint64 minutes = qMax<qint64>(1, (seconds + 59) / 60);

    if(minutes < 60)
        return chinese ? QStringLiteral("剩余 %1 分钟").arg(minutes)
                       : QStringLiteral("%1 min left").arg(minutes);

    const qreal hours = minutes / 60.0;
    return chinese ? QStringLiteral("剩余约 %1 小时").arg(hours, 0, 'f', hours < 10 ? 1 : 0)
                   : QStringLiteral("about %1 h left").arg(hours, 0, 'f', hours < 10 ? 1 : 0);
}

}
