#include "documentanalyzer.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace catalyst {

namespace {

void appendUnique(QVector<double>& values, double value) {
    if (!values.contains(value)) {
        values.append(value);
    }
}

void appendUnique(QStringList& values, const QString& value) {
    if (!value.isEmpty() && !values.contains(value, Qt::CaseInsensitive)) {
        values.append(value);
    }
}

QString decodeText(const QByteArray& bytes) {
    QByteArray data = bytes;
    if (data.startsWith("\xEF\xBB\xBF")) {
        data.remove(0, 3);
    }

    QString utf8 = QString::fromUtf8(data);
    if (!utf8.contains(QChar::ReplacementCharacter)) {
        return utf8;
    }

    const QString local = QString::fromLocal8Bit(data);
    if (local.count(QChar::ReplacementCharacter) < utf8.count(QChar::ReplacementCharacter)) {
        return local;
    }
    return utf8;
}

QString cleanDoi(QString value) {
    static const QString trailing = QStringLiteral(".,;)]}");
    value = value.trimmed();
    while (!value.isEmpty() && trailing.contains(value.back())) {
        value.chop(1);
    }
    return value;
}

} // namespace

bool DocumentAnalyzer::readTextFile(
    const QString& path,
    QString* text,
    QString* errorMessage) {
    if (!text) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("内部错误：未提供文本输出缓冲区。");
        }
        return false;
    }

    const QString suffix = QFileInfo(path).suffix().toCaseFolded();
    const QStringList supported = {
        QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("csv"), QStringLiteral("tsv")};
    if (!supported.contains(suffix)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "当前原生资料解析先支持 TXT、Markdown、CSV 和 TSV。PDF 仍由旧研究实现作为参考，原生 PDF 文本提取将在后续迁移。请勿把扫描 PDF 自动 OCR 后当作可靠实验事实。");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法打开资料文件：%1").arg(path);
        }
        return false;
    }

    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("资料文件为空：%1").arg(path);
        }
        return false;
    }

    *text = decodeText(bytes);
    if (text->trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("资料文件未解析到可读文本。");
        }
        return false;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("已读取 %1 个字符。")
                            .arg(text->size());
    }
    return true;
}

DocumentSignals DocumentAnalyzer::analyzeText(
    const QString& text,
    const QString& sourcePath) {
    DocumentSignals signals;
    signals.sourcePath = sourcePath;
    signals.characterCount = text.size();

    const QRegularExpression doiRe(
        QStringLiteral(R"(10\.\d{4,9}/[-._;()/:A-Z0-9]+)"),
        QRegularExpression::CaseInsensitiveOption);
    auto doiMatches = doiRe.globalMatch(text);
    while (doiMatches.hasNext() && signals.dois.size() < 20) {
        appendUnique(signals.dois, cleanDoi(doiMatches.next().captured(0)));
    }

    const QRegularExpression temperatureRe(
        QStringLiteral(R"((?<!\d)(\d{2,4}(?:\.\d+)?)\s*(?:°\s*C|℃|deg\.?\s*C))"),
        QRegularExpression::CaseInsensitiveOption);
    auto temperatureMatches = temperatureRe.globalMatch(text);
    while (temperatureMatches.hasNext() && signals.temperaturesC.size() < 50) {
        bool ok = false;
        const double value = temperatureMatches.next().captured(1).toDouble(&ok);
        if (ok) {
            appendUnique(signals.temperaturesC, value);
        }
    }
    std::sort(signals.temperaturesC.begin(), signals.temperaturesC.end());

    const QRegularExpression timeRe(
        QStringLiteral(R"((?<![\w.])(\d+(?:\.\d+)?)\s*(h|hr|hrs|hour|hours|min|mins|minute|minutes)(?!\w))"),
        QRegularExpression::CaseInsensitiveOption);
    auto timeMatches = timeRe.globalMatch(text);
    while (timeMatches.hasNext() && signals.durationsHours.size() < 100) {
        const auto match = timeMatches.next();
        bool ok = false;
        double value = match.captured(1).toDouble(&ok);
        if (!ok) {
            continue;
        }
        const QString unit = match.captured(2).toCaseFolded();
        if (!unit.startsWith(QLatin1Char('h'))) {
            value /= 60.0;
        }
        appendUnique(signals.durationsHours, value);
    }
    std::sort(signals.durationsHours.begin(), signals.durationsHours.end());

    const QRegularExpression conversionRe(
        QStringLiteral(R"((?:CH\s*4|CH4|methane)[^\n%]{0,80}?(\d+(?:\.\d+)?)\s*%)"),
        QRegularExpression::CaseInsensitiveOption);
    auto conversionMatches = conversionRe.globalMatch(text);
    while (conversionMatches.hasNext() && signals.ch4ConversionPercentCandidates.size() < 100) {
        bool ok = false;
        const double value = conversionMatches.next().captured(1).toDouble(&ok);
        if (ok) {
            signals.ch4ConversionPercentCandidates.append(value);
        }
    }

    const QMap<QString, QStringList> keywordSets = {
        {QStringLiteral("coking"), {
            QStringLiteral("coke"), QStringLiteral("coking"),
            QStringLiteral("carbon deposition"), QStringLiteral("积碳"), QStringLiteral("结焦")}},
        {QStringLiteral("sintering"), {
            QStringLiteral("sinter"), QStringLiteral("sintering"), QStringLiteral("烧结")}},
        {QStringLiteral("stability"), {
            QStringLiteral("stability"), QStringLiteral("stable"), QStringLiteral("deactivation"),
            QStringLiteral("time-on-stream"), QStringLiteral("TOS"), QStringLiteral("稳定"), QStringLiteral("失活")}},
        {QStringLiteral("regeneration"), {
            QStringLiteral("regeneration"), QStringLiteral("regenerated"), QStringLiteral("再生")}}
    };

    const QString folded = text.toCaseFolded();
    for (auto it = keywordSets.cbegin(); it != keywordSets.cend(); ++it) {
        QStringList detected;
        for (const QString& term : it.value()) {
            if (folded.contains(term.toCaseFolded())) {
                detected.append(term);
            }
        }
        if (!detected.isEmpty()) {
            signals.keywordEvidence.insert(it.key(), detected);
        }
    }

    signals.snippets = evidenceSnippets(text, defaultEvidenceTerms());
    return signals;
}

QVector<EvidenceSnippet> DocumentAnalyzer::evidenceSnippets(
    const QString& text,
    const QStringList& terms,
    int radius,
    int limit) {
    QVector<EvidenceSnippet> snippets;
    if (text.isEmpty() || terms.isEmpty() || limit <= 0) {
        return snippets;
    }

    radius = std::max(0, radius);
    const QString folded = text.toCaseFolded();

    for (const QString& rawTerm : terms) {
        const QString term = rawTerm.trimmed();
        const QString target = term.toCaseFolded();
        if (target.isEmpty()) {
            continue;
        }

        qsizetype start = 0;
        while (snippets.size() < limit) {
            const qsizetype index = folded.indexOf(target, start);
            if (index < 0) {
                break;
            }

            const qsizetype left = std::max<qsizetype>(0, index - radius);
            const qsizetype right = std::min<qsizetype>(text.size(), index + term.size() + radius);
            EvidenceSnippet snippet;
            snippet.term = term;
            snippet.snippet = text.mid(left, right - left).simplified();
            snippets.append(snippet);
            start = index + target.size();
        }
        if (snippets.size() >= limit) {
            break;
        }
    }

    return snippets;
}

QStringList DocumentAnalyzer::defaultEvidenceTerms() {
    return {
        QStringLiteral("stability"), QStringLiteral("deactivation"), QStringLiteral("time-on-stream"),
        QStringLiteral("coke"), QStringLiteral("coking"), QStringLiteral("sintering"),
        QStringLiteral("regeneration"), QStringLiteral("稳定"), QStringLiteral("失活"),
        QStringLiteral("积碳"), QStringLiteral("烧结"), QStringLiteral("再生")
    };
}

} // namespace catalyst
