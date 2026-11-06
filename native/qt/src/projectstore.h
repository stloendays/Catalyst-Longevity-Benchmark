#pragma once

#include "documentanalyzer.h"
#include "models.h"

#include <QString>
#include <QVector>

namespace catalyst {

class ProjectStore {
public:
    // 证据资料相关字段采用增量方式添加，旧版项目文件仍可正常打开。
    static constexpr int SchemaVersion = 1;

    static bool saveProject(
        const QString& path,
        const QVector<Record>& records,
        QString* errorMessage = nullptr);

    static bool saveProject(
        const QString& path,
        const QVector<Record>& records,
        const QVector<EvidenceItem>& evidenceItems,
        QString* errorMessage = nullptr);

    static bool loadProject(
        const QString& path,
        QVector<Record>* records,
        QString* errorMessage = nullptr);

    static bool loadProject(
        const QString& path,
        QVector<Record>* records,
        QVector<EvidenceItem>* evidenceItems,
        QString* errorMessage = nullptr);
};

} // catalyst 命名空间
