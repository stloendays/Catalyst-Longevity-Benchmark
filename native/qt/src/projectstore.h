#pragma once

#include "models.h"

#include <QString>
#include <QVector>

namespace catalyst {

class ProjectStore {
public:
    static constexpr int SchemaVersion = 1;

    static bool saveProject(
        const QString& path,
        const QVector<Record>& records,
        QString* errorMessage = nullptr);

    static bool loadProject(
        const QString& path,
        QVector<Record>* records,
        QString* errorMessage = nullptr);
};

} // namespace catalyst
