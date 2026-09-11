#include "AppLogger.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QUuid>

#include <cstdlib>

namespace {
constexpr qint64 kMaxPendingOperatorBytes = 512 * 1024;
constexpr int kMaxOperatorInputChars = 8192;

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

QString operatorPendingPath() {
    return QDir(AppLogger::operatorLogDirectory()).filePath(QStringLiteral("pending.jsonl"));
}

QString operatorOutboxDirectory() {
    return QDir(AppLogger::operatorLogDirectory()).filePath(QStringLiteral("outbox"));
}

QString operatorArchiveDirectory() {
    return QDir(AppLogger::operatorLogDirectory()).filePath(QStringLiteral("archive"));
}

QString sessionId() {
    static const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return id;
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

QString redactLikelySecrets(QString text) {
    text = text.left(kMaxOperatorInputChars);

    static const QRegularExpression namedSecret(
        QStringLiteral(R"(((?:api[_-]?key|token|password|passwd|secret)\s*[:=]\s*)[^\s,;]+)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression githubToken(
        QStringLiteral(R"(\bgh[pousr]_[A-Za-z0-9_]{20,}\b)"));
    static const QRegularExpression openAiToken(
        QStringLiteral(R"(\bsk-[A-Za-z0-9_-]{16,}\b)"));
    static const QRegularExpression jwt(
        QStringLiteral(R"(\b[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{10,}\b)"));

    text.replace(namedSecret, QStringLiteral("\\1[REDACTED]"));
    text.replace(githubToken, QStringLiteral("[REDACTED_GITHUB_TOKEN]"));
    text.replace(openAiToken, QStringLiteral("[REDACTED_API_KEY]"));
    text.replace(jwt, QStringLiteral("[REDACTED_JWT]"));
    return text;
}

bool validBatchId(const QString &batchId) {
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]{64}$"));
    return re.match(batchId).hasMatch();
}

QString sealPendingOperatorLogUnlocked() {
    const QString pendingPath = operatorPendingPath();
    QFile pending(pendingPath);
    if(!pending.exists() || pending.size() <= 0) return {};
    if(!pending.open(QIODevice::ReadOnly)) return {};

    const QByteArray raw = pending.readAll();
    pending.close();
    if(raw.trimmed().isEmpty()) {
        QFile::remove(pendingPath);
        return {};
    }

    const QString batchId = QString::fromLatin1(
        QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toHex());
    QDir().mkpath(operatorOutboxDirectory());
    const QString destination =
        QDir(operatorOutboxDirectory()).filePath(batchId + QStringLiteral(".jsonl"));

    if(QFileInfo::exists(destination)) {
        QFile::remove(pendingPath);
        return batchId;
    }

    if(QFile::rename(pendingPath, destination)) return batchId;
    if(QFile::copy(pendingPath, destination)) {
        QFile::remove(pendingPath);
        return batchId;
    }
    return {};
}
}

namespace AppLogger {
void install() {
    QDir().mkpath(logDirectory());
    QDir().mkpath(operatorLogDirectory());
    QDir().mkpath(operatorOutboxDirectory());
    QDir().mkpath(operatorArchiveDirectory());
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

QString operatorLogDirectory() {
    return QDir(logDirectory()).filePath(QStringLiteral("operator"));
}

void recordOperatorEvent(const QString &event,
                         const QString &input,
                         const QJsonObject &details) {
    const QString normalizedEvent = event.trimmed().left(80);
    if(normalizedEvent.isEmpty()) return;

    QMutexLocker locker(&logMutex());
    QDir().mkpath(operatorLogDirectory());
    QDir().mkpath(operatorOutboxDirectory());
    QDir().mkpath(operatorArchiveDirectory());

    QFileInfo pendingInfo(operatorPendingPath());
    if(pendingInfo.exists() && pendingInfo.size() >= kMaxPendingOperatorBytes)
        sealPendingOperatorLogUnlocked();

    QJsonObject record{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("event_id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("ts_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("session_id"), sessionId()},
        {QStringLiteral("event"), normalizedEvent},
        {QStringLiteral("app_version"), QCoreApplication::applicationVersion()}
    };
    if(!input.isEmpty())
        record.insert(QStringLiteral("input"), redactLikelySecrets(input));
    if(!details.isEmpty())
        record.insert(QStringLiteral("details"), details);

    QFile file(operatorPendingPath());
    if(!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    file.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
    file.write("\n");
    file.flush();
}

QStringList prepareOperatorLogUploadBatches() {
    QMutexLocker locker(&logMutex());
    QDir().mkpath(operatorOutboxDirectory());
    sealPendingOperatorLogUnlocked();

    QStringList batchIds;
    const QDir outbox(operatorOutboxDirectory());
    const QStringList files =
        outbox.entryList(QStringList{QStringLiteral("*.jsonl")}, QDir::Files, QDir::Name);
    for(const QString &fileName : files) {
        const QString batchId = QFileInfo(fileName).completeBaseName().toLower();
        if(validBatchId(batchId)) batchIds.push_back(batchId);
    }
    return batchIds;
}

QByteArray readOperatorLogBatch(const QString &batchId, int maxBytes) {
    const QString normalized = batchId.trimmed().toLower();
    if(!validBatchId(normalized) || maxBytes <= 0) return {};

    QMutexLocker locker(&logMutex());
    QFile file(QDir(operatorOutboxDirectory()).filePath(normalized + QStringLiteral(".jsonl")));
    if(!file.open(QIODevice::ReadOnly)) return {};
    if(file.size() > maxBytes) return {};
    return file.readAll();
}

bool markOperatorLogUploaded(const QString &batchId) {
    const QString normalized = batchId.trimmed().toLower();
    if(!validBatchId(normalized)) return false;

    QMutexLocker locker(&logMutex());
    const QString source =
        QDir(operatorOutboxDirectory()).filePath(normalized + QStringLiteral(".jsonl"));
    if(!QFileInfo::exists(source)) return false;

    const QString month = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM"));
    const QString archiveDir = QDir(operatorArchiveDirectory()).filePath(month);
    QDir().mkpath(archiveDir);
    const QString destination =
        QDir(archiveDir).filePath(normalized + QStringLiteral(".jsonl"));

    if(QFileInfo::exists(destination)) {
        QFile::remove(source);
        return true;
    }
    if(QFile::rename(source, destination)) return true;
    if(QFile::copy(source, destination)) {
        QFile::remove(source);
        return true;
    }
    return false;
}

QString readRecent(int maxBytes) {
    QMutexLocker locker(&logMutex());
    QString result = tailOf(logFilePath(), maxBytes);
    const QString update = tailOf(updateLogPath(), qMin(maxBytes / 2, 96 * 1024));
    if(!update.isEmpty()) {
        if(!result.isEmpty()) result += QStringLiteral("\n\n");
        result += QStringLiteral("===== updater =====\n") + update;
    }

    const QString operatorInput =
        tailOf(operatorPendingPath(), qMin(maxBytes / 2, 96 * 1024));
    if(!operatorInput.isEmpty()) {
        if(!result.isEmpty()) result += QStringLiteral("\n\n");
        result += QStringLiteral("===== operator input (local pending) =====\n") + operatorInput;
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
    // Operator behavior logs are intentionally retained. They are the local source
    // of truth requested for later encrypted archival and should not disappear
    // when clearing diagnostic application logs.
    return ok;
}
}
