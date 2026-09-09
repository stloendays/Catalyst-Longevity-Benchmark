#pragma once

#include "models.h"

#include <QString>
#include <QVector>
#include <optional>

namespace catalyst {

enum class ReferenceKind {
    Compound,
    Literature
};

struct ReferenceEntry {
    ReferenceKind kind = ReferenceKind::Literature;
    QString source;
    QString title;
    QString identifier;
    QString keyFacts;
    QString use;
    QString sourceUrl;
    QString snapshotDate;
};

struct ExperimentReference {
    QString id;
    QString citation;
    QString doi;
    QString reaction;
    QString catalystFamily;
    std::optional<double> temperatureC;
    std::optional<double> ghsv;
    std::optional<double> pressureBar;
    QString feed;
    double durationHours = 0.0;
    QString role;
    QString sourceUrl;
};

struct ReferenceMatch {
    ExperimentReference reference;
    int relevanceScore = 0;
    QString reason;
};

class ReferenceKnowledgeBase final {
public:
    static QVector<ReferenceEntry> entries();
    static QVector<ExperimentReference> experimentReferences();
    static QVector<ReferenceMatch> matchExperimentContext(const QVector<Record>& records);
    static QString compactMatchText(const QVector<ReferenceMatch>& matches, int limit = 2);
};

} // namespace catalyst
