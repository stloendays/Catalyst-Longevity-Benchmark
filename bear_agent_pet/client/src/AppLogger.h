#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace AppLogger {
void install();
QString logDirectory();
QString logFilePath();
QString operatorLogDirectory();
void recordOperatorEvent(const QString &event,
                         const QString &input = {},
                         const QJsonObject &details = {});
QStringList prepareOperatorLogUploadBatches();
QByteArray readOperatorLogBatch(const QString &batchId, int maxBytes = 768 * 1024);
bool markOperatorLogUploaded(const QString &batchId);
QString readRecent(int maxBytes = 256 * 1024);
bool clear();
}
