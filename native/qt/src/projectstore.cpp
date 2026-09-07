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
    if (target) {
        *target = message;
    }
}

QVariant optionalVariant(const std::optional<double>& value) {
    return value.has_value() ? QVariant(*value) : QVariant();
}

std::optional<double> optionalDouble(const QVariant& value) {
    return value.isNull() ? std::nullopt : std::optional<double>(value.toDouble());
}

bool execOrSetError(QSqlQuery& query, QString* errorMessage, const QString& context) {
    if (query.exec()) {
        return true;
    }
    setError(errorMessage, QStringLiteral("%1：%2").arg(context, query.lastError().text()));
    return false;
}

} // namespace

bool ProjectStore::saveProject(
    const QString& path,
    const QVector<Record>& records,
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
            if (!execOrSetError(query, errorMessage,
                    QStringLiteral("PRAGMA foreign_keys = ON"))) {
                db.close();
            } else if (!execOrSetError(query, errorMessage,
                    QStringLiteral("CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL)"))) {
                db.close();
            } else if (!execOrSetError(query, errorMessage,
                    QStringLiteral(
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
                        "source TEXT)"))) {
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
                    QSqlQuery metadata(db);
                    metadata.prepare(QStringLiteral("INSERT INTO metadata(key, value) VALUES(?, ?)"));
                    const QList<QPair<QString, QString>> entries = {
                        {QStringLiteral("schema_version"), QString::number(SchemaVersion)},
                        {QStringLiteral("product"), QStringLiteral("Catalyst Longevity Research")},
                        {QStringLiteral("saved_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                        {QStringLiteral("project_name"), QFileInfo(path).completeBaseName()}
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
                    if (!db.commit()) {
                        setError(errorMessage, QStringLiteral("保存项目失败：%1").arg(db.lastError().text()));
                        db.rollback();
                    } else {
                        success = true;
                        setError(errorMessage,
                            QStringLiteral("项目已保存：%1 条实验记录。").arg(records.size()));
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
    if (!records) {
        setError(errorMessage, QStringLiteral("项目加载目标无效。"));
        return false;
    }
    records->clear();

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
                QSqlQuery query(db);
                if (!query.exec(QStringLiteral(
                        "SELECT catalyst, time_h, performance, temperature_c, ghsv, whsv, pressure_bar, feed_ratio, metric, source "
                        "FROM observations ORDER BY id"))) {
                    setError(errorMessage,
                        QStringLiteral("读取项目数据失败：%1").arg(query.lastError().text()));
                } else {
                    QVector<Record> loaded;
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
                        loaded.append(record);
                    }
                    *records = loaded;
                    success = true;
                    setError(errorMessage,
                        QStringLiteral("项目已打开：%1 条实验记录。").arg(records->size()));
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

} // namespace catalyst
