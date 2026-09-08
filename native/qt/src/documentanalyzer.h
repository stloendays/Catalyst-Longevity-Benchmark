#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace catalyst {

struct EvidenceSnippet {
    QString term;
    QString snippet;
};

struct EvidenceItem {
    QString sourcePath;
    QString sourceSha256;
    QString category;
    QString term;
    QString valueText;
    QString snippet;
    QString boundCatalyst;
    std::optional<double> boundTimeHours;
    QString status = QStringLiteral("candidate_requires_condition_binding");
    QString note;
};

struct DocumentSignals {
    QString sourcePath;
    QString sourceSha256;
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

    static QVector<EvidenceItem> candidateItems(
        const QString& text,
        const DocumentSignals& document);

    static QVector<EvidenceSnippet> evidenceSnippets(
        const QString& text,
        const QStringList& terms,
        int radius = 140,
        int limit = 12);

    static QStringList defaultEvidenceTerms();
    static QString bindingStatus(
        const QString& catalyst,
        const std::optional<double>& timeHours);
};

} // namespace catalyst
