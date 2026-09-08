#pragma once

#include "documentanalyzer.h"
#include "models.h"

#include <QString>
#include <QVector>

namespace catalyst {

class ProjectStore {
public:
    // Keep schema version 1 backward compatible. Evidence persistence is an
    // additive table so older .clrproj files remain readable.
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

} // namespace catalyst
