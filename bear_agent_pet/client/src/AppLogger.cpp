#include "AppLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>

#include <cstdlib>

namespace {
QMutex &logMutex() {
    static QMutex mutex;
    return mutex;
}

QString levelName(QtMsgType type) {
    switch(type) {
    case QtDebugMsg: return QStringLiteral("DEBUG");
    case QtInfoMsg: return QStringLiteral("INFO");
    case QtWarningMsg: return QStringLiteral("WARN");
    case QtCriticalMsg: return QStringLiteral("ERROR");
    case QtFatalMsg: return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

QString updateLogPath() {
    return AppLogger::logDirectory() + QStringLiteral("/tony-update.log");
}

void rotateIfNeeded(const QString &path) {
    QFileInfo info(path);
    if(!info.exists() || info.size() < 2 * 1024 * 1024) return;
    const QString oldPath = path + QStringLiteral(".1");
    QFile::remove(oldPath);
    QFile::rename(path, oldPath);
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    QMutexLocker locker(&logMutex());
    const QString dir = AppLogger::logDirectory();
    QDir().mkpath(dir);
    const QString path = AppLogger::logFilePath();
    rotateIfNeeded(path);

    QFile file(path);
    if(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
            << " [" << levelName(type) << "]";
        if(context.category && *context.category)
            out << " [" << context.category << "]";
        out << ' ' << message << '\n';
    }

    if(type == QtFatalMsg) std::abort();
}

QString tailOf(const QString &path, int maxBytes) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return {};
    const qint64 size = file.size();
    if(size > maxBytes) file.seek(size - maxBytes);
    QByteArray data = file.readAll();
    if(size > maxBytes) {
        const int firstNewline = data.indexOf('\n');
        if(firstNewline >= 0) data.remove(0, firstNewline + 1);
    }
    return QString::fromUtf8(data);
}
}

namespace AppLogger {
void install() {
    QDir().mkpath(logDirectory());
    qInstallMessageHandler(messageHandler);
}

QString logDirectory() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if(base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.tony-desktop-pet");
    return QDir(base).filePath(QStringLiteral("logs"));
}

QString logFilePath() {
    return QDir(logDirectory()).filePath(QStringLiteral("tony.log"));
}

QString readRecent(int maxBytes) {
    QMutexLocker locker(&logMutex());
    QString result = tailOf(logFilePath(), maxBytes);
    const QString update = tailOf(updateLogPath(), qMin(maxBytes / 2, 96 * 1024));
    if(!update.isEmpty()) {
        if(!result.isEmpty()) result += QStringLiteral("\n\n");
        result += QStringLiteral("===== updater =====\n") + update;
    }
    return result;
}

bool clear() {
    QMutexLocker locker(&logMutex());
    bool ok = true;
    QFile file(logFilePath());
    if(file.exists()) ok = file.open(QIODevice::WriteOnly | QIODevice::Truncate) && ok;
    QFile update(updateLogPath());
    if(update.exists()) ok = update.open(QIODevice::WriteOnly | QIODevice::Truncate) && ok;
    return ok;
}
}
