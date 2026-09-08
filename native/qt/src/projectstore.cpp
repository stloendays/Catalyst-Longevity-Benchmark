#include "projectstore.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace catalyst {

namespace {

QString newConnectionName() {
    return QStringLiteral("catalyst_project_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void setError(QString* target, const QString& message) {
    if (target) *target = message;
}

QVariant optionalVariant(const std::optional<double>& value) {
    return value.has_value() ? QVariant(*value) : QVariant();
}

std::optional<double> optionalDouble(const QVariant& value) {
    return value.isNull() ? std::nullopt : std::optional<double>(value.toDouble());
}

bool execOrSetError(QSqlQuery& query, QString* errorMessage, const QString& sql) {
    if (query.exec(sql)) return true;
    setError(errorMessage, QStringLiteral("%1：%2").arg(sql, query.lastError().text()));
    return false;
}

bool tableExists(QSqlDatabase& db, const QString& tableName) {
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1"));
    query.addBindValue(tableName);
    return query.exec() && query.next();
}

} // namespace

bool ProjectStore::saveProject(
    const QString& path,
    const QVector<Record>& records,
    QString* errorMessage) {
    return saveProject(path, records, {}, errorMessage);
}

bool ProjectStore::saveProject(
    const QString& path,
    const QVector<Record>& records,
    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage) {
    if (path.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("项目文件路径为空。"));
        return false;
    }

    const QString connectionName = newConnectionName();
    bool success = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        if (!db.open()) {
            setError(errorMessage, QStringLiteral("无法创建项目文件：%1").arg(db.lastError().text()));
        } else {
            QSqlQuery query(db);
            const QString metadataSql = QStringLiteral(
                "CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL)");
            const QString observationsSql = QStringLiteral(
                "CREATE TABLE IF NOT EXISTS observations ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "catalyst TEXT NOT NULL, "
                "time_h REAL NOT NULL, "
                "performance REAL NOT NULL, "
                "temperature_c REAL, "
                "ghsv REAL, "
                "whsv REAL, "
                "pressure_bar REAL, "
                "feed_ratio TEXT, "
                "metric TEXT, "
                "source TEXT)");
            const QString evidenceSql = QStringLiteral(
                "CREATE TABLE IF NOT EXISTS evidence_items ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "source_path TEXT, "
                "source_sha256 TEXT, "
                "category TEXT NOT NULL, "
                "term TEXT, "
                "value_text TEXT, "
                "snippet TEXT, "
                "bound_catalyst TEXT, "
                "bound_time_h REAL, "
                "status TEXT NOT NULL, "
                "note TEXT)");

            if (!execOrSetError(query, errorMessage, QStringLiteral("PRAGMA foreign_keys = ON"))
                || !execOrSetError(query, errorMessage, metadataSql)
                || !execOrSetError(query, errorMessage, observationsSql)
                || !execOrSetError(query, errorMessage, evidenceSql)) {
                db.close();
            } else if (!db.transaction()) {
                setError(errorMessage, QStringLiteral("无法开始保存事务：%1").arg(db.lastError().text()));
                db.close();
            } else {
                bool ok = true;
                QSqlQuery clearMetadata(db);
                ok = execOrSetError(clearMetadata, errorMessage, QStringLiteral("DELETE FROM metadata"));

                if (ok) {
                    QSqlQuery clearObservations(db);
                    ok = execOrSetError(clearObservations, errorMessage, QStringLiteral("DELETE FROM observations"));
                }
                if (ok) {
                    QSqlQuery clearEvidence(db);
                    ok = execOrSetError(clearEvidence, errorMessage, QStringLiteral("DELETE FROM evidence_items"));
                }

                if (ok) {
                    QSqlQuery metadata(db);
                    metadata.prepare(QStringLiteral("INSERT INTO metadata(key, value) VALUES(?, ?)"));
                    const QList<QPair<QString, QString>> entries = {
                        {QStringLiteral("schema_version"), QString::number(SchemaVersion)},
                        {QStringLiteral("product"), QStringLiteral("Catalyst Longevity Research")},
                        {QStringLiteral("saved_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                        {QStringLiteral("project_name"), QFileInfo(path).completeBaseName()},
                        {QStringLiteral("evidence_model"), QStringLiteral("candidate_binding_v1")}
                    };
                    for (const auto& entry : entries) {
                        metadata.bindValue(0, entry.first);
                        metadata.bindValue(1, entry.second);
                        if (!metadata.exec()) {
                            setError(errorMessage,
                                QStringLiteral("写入项目元数据失败：%1").arg(metadata.lastError().text()));
                            ok = false;
                            break;
                        }
                    }
                }

                if (ok) {
                    QSqlQuery insert(db);
                    insert.prepare(QStringLiteral(
                        "INSERT INTO observations("
                        "catalyst, time_h, performance, temperature_c, ghsv, whsv, pressure_bar, feed_ratio, metric, source"
                        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
                    for (const auto& record : records) {
                        insert.bindValue(0, record.catalyst);
                        insert.bindValue(1, record.timeHours);
                        insert.bindValue(2, record.performance);
                        insert.bindValue(3, optionalVariant(record.temperatureC));
                        insert.bindValue(4, optionalVariant(record.gHSV));
                        insert.bindValue(5, optionalVariant(record.wHSV));
                        insert.bindValue(6, optionalVariant(record.pressure));
                        insert.bindValue(7, record.feedRatio);
                        insert.bindValue(8, record.metric);
                        insert.bindValue(9, record.source);
                        if (!insert.exec()) {
                            setError(errorMessage,
                                QStringLiteral("写入实验记录失败：%1").arg(insert.lastError().text()));
                            ok = false;
                            break;
                        }
                    }
                }

                if (ok) {
                    QSqlQuery insertEvidence(db);
                    insertEvidence.prepare(QStringLiteral(
                        "INSERT INTO evidence_items("
                        "source_path, source_sha256, category, term, value_text, snippet, "
                        "bound_catalyst, bound_time_h, status, note"
                        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
                    for (const auto& item : evidenceItems) {
                        insertEvidence.bindValue(0, item.sourcePath);
                        insertEvidence.bindValue(1, item.sourceSha256);
                        insertEvidence.bindValue(2, item.category);
                        insertEvidence.bindValue(3, item.term);
                        insertEvidence.bindValue(4, item.valueText);
                        insertEvidence.bindValue(5, item.snippet);
                        insertEvidence.bindValue(6, item.boundCatalyst);
                        insertEvidence.bindValue(7, optionalVariant(item.boundTimeHours));
                        insertEvidence.bindValue(8, item.status);
                        insertEvidence.bindValue(9, item.note);
                        if (!insertEvidence.exec()) {
                            setError(errorMessage,
                                QStringLiteral("写入证据候选失败：%1").arg(insertEvidence.lastError().text()));
                            ok = false;
                            break;
                        }
                    }
                }

                if (ok) {
                    if (!db.commit()) {
                        setError(errorMessage, QStringLiteral("保存项目失败：%1").arg(db.lastError().text()));
                        db.rollback();
                    } else {
                        success = true;
                        setError(errorMessage,
                            QStringLiteral("项目已保存：%1 条实验记录，%2 条证据候选。")
                                .arg(records.size())
                                .arg(evidenceItems.size()));
                    }
                } else {
                    db.rollback();
                }
                db.close();
            }
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

bool ProjectStore::loadProject(
    const QString& path,
    QVector<Record>* records,
    QString* errorMessage) {
    QVector<EvidenceItem> ignored;
    return loadProject(path, records, &ignored, errorMessage);
}

bool ProjectStore::loadProject(
    const QString& path,
    QVector<Record>* records,
    QVector<EvidenceItem>* evidenceItems,
    QString* errorMessage) {
    if (!records || !evidenceItems) {
        setError(errorMessage, QStringLiteral("项目加载目标无效。"));
        return false;
    }
    records->clear();
    evidenceItems->clear();

    if (path.trimmed().isEmpty() || !QFileInfo::exists(path)) {
        setError(errorMessage, QStringLiteral("项目文件不存在。"));
        return false;
    }

    const QString connectionName = newConnectionName();
    bool success = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!db.open()) {
            setError(errorMessage, QStringLiteral("无法打开项目文件：%1").arg(db.lastError().text()));
        } else {
            QSqlQuery versionQuery(db);
            versionQuery.prepare(QStringLiteral("SELECT value FROM metadata WHERE key = 'schema_version'"));
            if (!versionQuery.exec() || !versionQuery.next()) {
                setError(errorMessage, QStringLiteral("这不是有效的 Catalyst Longevity Research 项目文件。"));
            } else if (versionQuery.value(0).toInt() != SchemaVersion) {
                setError(errorMessage,
                    QStringLiteral("项目数据库版本不兼容：%1（当前支持 %2）。")
                        .arg(versionQuery.value(0).toString())
                        .arg(SchemaVersion));
            } else {
                bool ok = true;
                QSqlQuery query(db);
                if (!query.exec(QStringLiteral(
                        "SELECT catalyst, time_h, performance, temperature_c, ghsv, whsv, pressure_bar, feed_ratio, metric, source "
                        "FROM observations ORDER BY id"))) {
                    setError(errorMessage,
                        QStringLiteral("读取项目数据失败：%1").arg(query.lastError().text()));
                    ok = false;
                } else {
                    while (query.next()) {
                        Record record;
                        record.catalyst = query.value(0).toString();
                        record.timeHours = query.value(1).toDouble();
                        record.performance = query.value(2).toDouble();
                        record.temperatureC = optionalDouble(query.value(3));
                        record.gHSV = optionalDouble(query.value(4));
                        record.wHSV = optionalDouble(query.value(5));
                        record.pressure = optionalDouble(query.value(6));
                        record.feedRatio = query.value(7).toString();
                        record.metric = query.value(8).toString();
                        record.source = query.value(9).toString();
                        records->append(record);
                    }
                }

                // Evidence is additive under schema version 1. Old projects that
                // predate this table remain valid and simply load zero items.
                if (ok && tableExists(db, QStringLiteral("evidence_items"))) {
                    QSqlQuery evidenceQuery(db);
                    if (!evidenceQuery.exec(QStringLiteral(
                            "SELECT source_path, source_sha256, category, term, value_text, snippet, "
                            "bound_catalyst, bound_time_h, status, note "
                            "FROM evidence_items ORDER BY id"))) {
                        setError(errorMessage,
                            QStringLiteral("读取项目证据失败：%1").arg(evidenceQuery.lastError().text()));
                        ok = false;
                    } else {
                        while (evidenceQuery.next()) {
                            EvidenceItem item;
                            item.sourcePath = evidenceQuery.value(0).toString();
                            item.sourceSha256 = evidenceQuery.value(1).toString();
                            item.category = evidenceQuery.value(2).toString();
                            item.term = evidenceQuery.value(3).toString();
                            item.valueText = evidenceQuery.value(4).toString();
                            item.snippet = evidenceQuery.value(5).toString();
                            item.boundCatalyst = evidenceQuery.value(6).toString();
                            item.boundTimeHours = optionalDouble(evidenceQuery.value(7));
                            item.status = evidenceQuery.value(8).toString();
                            item.note = evidenceQuery.value(9).toString();
                            evidenceItems->append(item);
                        }
                    }
                }

                if (ok) {
                    success = true;
                    setError(errorMessage,
                        QStringLiteral("项目已打开：%1 条实验记录，%2 条证据候选。")
                            .arg(records->size())
                            .arg(evidenceItems->size()));
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

} // namespace catalyst
