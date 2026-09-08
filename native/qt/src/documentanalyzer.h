#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace catalyst {

struct EvidenceSnippet {
    QString term;
    QString snippet;
};

struct DocumentSignals {
    QString sourcePath;
    qsizetype characterCount = 0;
    QStringList dois;
    QVector<double> temperaturesC;
    QVector<double> durationsHours;
    QVector<double> ch4ConversionPercentCandidates;
    QMap<QString, QStringList> keywordEvidence;
    QVector<EvidenceSnippet> snippets;
};

class DocumentAnalyzer {
public:
    static bool readTextFile(
        const QString& path,
        QString* text,
        QString* errorMessage = nullptr);

    static DocumentSignals analyzeText(
        const QString& text,
        const QString& sourcePath = QString());

    static QVector<EvidenceSnippet> evidenceSnippets(
        const QString& text,
        const QStringList& terms,
        int radius = 140,
        int limit = 12);

    static QStringList defaultEvidenceTerms();
};

} // namespace catalyst
