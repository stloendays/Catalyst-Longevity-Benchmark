#include "documentanalyzer.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QRegularExpression>
#include <algorithm>

namespace catalyst {

namespace {

void appendUnique(QVector<double>& values, double value) {
    if (!values.contains(value)) values.append(value);
}

void appendUnique(QStringList& values, const QString& value) {
    if (!value.isEmpty() && !values.contains(value, Qt::CaseInsensitive)) values.append(value);
}

QString decodeText(const QByteArray& bytes) {
    QByteArray data = bytes;
    if (data.startsWith("\xEF\xBB\xBF")) data.remove(0, 3);

    QString utf8 = QString::fromUtf8(data);
    if (!utf8.contains(QChar::ReplacementCharacter)) return utf8;

    const QString local = QString::fromLocal8Bit(data);
    if (local.count(QChar::ReplacementCharacter) < utf8.count(QChar::ReplacementCharacter)) return local;
    return utf8;
}

QString cleanDoi(QString value) {
    static const QString trailing = QStringLiteral(".,;)]}");
    value = value.trimmed();
    while (!value.isEmpty() && trailing.contains(value.back())) value.chop(1);
    return value;
}

int pageForIndex(const QVector<qsizetype>& pageStartOffsets, qsizetype index) {
    if (index < 0 || pageStartOffsets.isEmpty()) return -1;
    int page = 0;
    for (qsizetype i = 0; i < pageStartOffsets.size(); ++i) {
        if (pageStartOffsets[i] <= index) page = static_cast<int>(i) + 1;
        else break;
    }
    return page > 0 ? page : -1;
}

QString contextAroundIndex(const QString& text, qsizetype index, qsizetype needleLength, int radius = 120) {
    if (text.isEmpty() || index < 0) return QString();
    const qsizetype left = std::max<qsizetype>(0, index - radius);
    const qsizetype right = std::min<qsizetype>(text.size(), index + needleLength + radius);
    return text.mid(left, right - left).simplified();
}

QString contextAround(const QString& text, const QString& needle, int radius = 120) {
    if (text.isEmpty() || needle.isEmpty()) return QString();
    const qsizetype index = text.indexOf(needle, 0, Qt::CaseInsensitive);
    return contextAroundIndex(text, index, needle.size(), radius);
}

QString pdfErrorText(QPdfDocument::Error error) {
    switch (error) {
    case QPdfDocument::Error::None:
        return QStringLiteral("无错误");
    case QPdfDocument::Error::DataNotYetAvailable:
        return QStringLiteral("PDF 数据尚未加载完成");
    case QPdfDocument::Error::FileNotFound:
        return QStringLiteral("PDF 文件不存在");
    case QPdfDocument::Error::InvalidFileFormat:
        return QStringLiteral("PDF 文件格式无效");
    case QPdfDocument::Error::IncorrectPassword:
        return QStringLiteral("PDF 需要密码或密码不正确");
    case QPdfDocument::Error::UnsupportedSecurityScheme:
        return QStringLiteral("PDF 使用了当前 Qt PDF 不支持的安全方案");
    case QPdfDocument::Error::Unknown:
    default:
        return QStringLiteral("未知 PDF 读取错误");
    }
}

QVector<EvidenceSnippet> evidenceSnippetsWithPages(
    const QString& text,
    const QStringList& terms,
    const QVector<qsizetype>& pageStartOffsets,
    int radius,
    int limit) {
    QVector<EvidenceSnippet> snippets;
    if (text.isEmpty() || terms.isEmpty() || limit <= 0) return snippets;

    radius = std::max(0, radius);
    const QString folded = text.toCaseFolded();

    for (const QString& rawTerm : terms) {
        const QString term = rawTerm.trimmed();
        const QString target = term.toCaseFolded();
        if (target.isEmpty()) continue;

        qsizetype start = 0;
        while (snippets.size() < limit) {
            const qsizetype index = folded.indexOf(target, start);
            if (index < 0) break;

            EvidenceSnippet snippet;
            snippet.term = term;
            snippet.snippet = contextAroundIndex(text, index, term.size(), radius);
            snippet.sourcePage = pageForIndex(pageStartOffsets, index);
            snippets.append(snippet);
            start = index + target.size();
        }
        if (snippets.size() >= limit) break;
    }
    return snippets;
}

void addCandidate(
    QVector<EvidenceItem>& items,
    const DocumentSignals& document,
    const QString& category,
    const QString& term,
    const QString& value,
    const QString& snippet,
    int sourcePage) {
    EvidenceItem item;
    item.sourcePath = document.sourcePath;
    item.sourceSha256 = document.sourceSha256;
    item.sourcePage = sourcePage;
    item.category = category;
    item.term = term;
    item.valueText = value;
    item.snippet = snippet;
    item.status = QStringLiteral("candidate_requires_condition_binding");

    const bool duplicate = std::any_of(items.cbegin(), items.cend(), [&](const EvidenceItem& existing) {
        return existing.sourceSha256 == item.sourceSha256
            && existing.sourcePage == item.sourcePage
            && existing.category == item.category
            && existing.term == item.term
            && existing.valueText == item.valueText
            && existing.snippet == item.snippet;
    });
    if (!duplicate) items.append(item);
}

} // namespace

bool DocumentAnalyzer::readDocument(
    const QString& path,
    DocumentReadResult* result,
    QString* errorMessage) {
    if (!result) {
        if (errorMessage) *errorMessage = QStringLiteral("内部错误：未提供资料读取结果缓冲区。");
        return false;
    }
    *result = DocumentReadResult{};

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        if (errorMessage) *errorMessage = QStringLiteral("资料文件不存在：%1").arg(path);
        return false;
    }

    QFile rawFile(path);
    if (!rawFile.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("无法打开资料文件：%1").arg(path);
        return false;
    }
    const QByteArray rawBytes = rawFile.readAll();
    rawFile.close();
    if (rawBytes.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("资料文件为空：%1").arg(path);
        return false;
    }

    result->sourcePath = path;
    result->sourceSha256 = QString::fromLatin1(
        QCryptographicHash::hash(rawBytes, QCryptographicHash::Sha256).toHex());

    const QString suffix = info.suffix().toCaseFolded();
    if (suffix == QStringLiteral("pdf")) {
        QPdfDocument pdf(nullptr);
        const QPdfDocument::Error pdfError = pdf.load(path);
        if (pdfError != QPdfDocument::Error::None) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("PDF 读取失败：%1。").arg(pdfErrorText(pdfError));
            }
            return false;
        }

        result->sourceFormat = QStringLiteral("pdf");
        result->pageCount = pdf.pageCount();
        if (result->pageCount <= 0) {
            if (errorMessage) *errorMessage = QStringLiteral("PDF 未包含可读取页面。");
            return false;
        }

        QString combined;
        int pagesWithText = 0;
        result->pageStartOffsets.reserve(result->pageCount);
        for (int page = 0; page < result->pageCount; ++page) {
            result->pageStartOffsets.append(combined.size());
            const QPdfSelection selection = pdf.getAllText(page);
            const QString pageText = selection.text();
            if (!pageText.trimmed().isEmpty()) ++pagesWithText;
            combined += pageText;
            combined += QStringLiteral("\n\n");
        }
        result->text = combined;

        if (result->text.trimmed().isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "PDF 没有可提取的文本层。它可能是扫描版论文；当前桌面版不会自动 OCR，避免把 OCR 误读直接当作实验事实。请使用带文本层的 PDF 或先人工核对后转成文本。 ");
            }
            return false;
        }
        if (pagesWithText < result->pageCount) {
            result->warning = QStringLiteral(
                "PDF 共 %1 页，其中 %2 页提取到文本；空白页或扫描页不会自动 OCR。")
                .arg(result->pageCount)
                .arg(pagesWithText);
        }

        if (errorMessage) {
            *errorMessage = QStringLiteral("已读取 PDF：%1 页，提取 %2 个字符。")
                .arg(result->pageCount)
                .arg(result->text.size());
        }
        return true;
    }

    const QStringList supported = {
        QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("csv"), QStringLiteral("tsv")};
    if (!supported.contains(suffix)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前支持 PDF、TXT、Markdown、CSV 和 TSV 资料文件。");
        }
        return false;
    }

    result->sourceFormat = suffix;
    result->pageCount = 1;
    result->pageStartOffsets = {0};
    result->text = decodeText(rawBytes);
    if (result->text.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("资料文件未解析到可读文本。");
        return false;
    }

    if (errorMessage) *errorMessage = QStringLiteral("已读取 %1 个字符。").arg(result->text.size());
    return true;
}

bool DocumentAnalyzer::readTextFile(
    const QString& path,
    QString* text,
    QString* errorMessage) {
    if (!text) {
        if (errorMessage) *errorMessage = QStringLiteral("内部错误：未提供文本输出缓冲区。");
        return false;
    }
    DocumentReadResult result;
    if (!readDocument(path, &result, errorMessage)) return false;
    *text = result.text;
    return true;
}

DocumentSignals DocumentAnalyzer::analyzeDocument(const DocumentReadResult& document) {
    DocumentSignals result = analyzeText(document.text, document.sourcePath);
    result.sourceSha256 = document.sourceSha256;
    result.sourceFormat = document.sourceFormat;
    result.pageCount = document.pageCount > 0 ? document.pageCount : 1;
    result.pageStartOffsets = document.pageStartOffsets.isEmpty()
        ? QVector<qsizetype>{0}
        : document.pageStartOffsets;
    result.snippets = evidenceSnippetsWithPages(
        document.text, defaultEvidenceTerms(), result.pageStartOffsets, 140, 12);
    return result;
}

DocumentSignals DocumentAnalyzer::analyzeText(
    const QString& text,
    const QString& sourcePath) {
    DocumentSignals result;
    result.sourcePath = sourcePath;
    result.sourceSha256 = QString::fromLatin1(
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
    result.sourceFormat = QStringLiteral("text");
    result.pageCount = 1;
    result.pageStartOffsets = {0};
    result.characterCount = text.size();

    const QRegularExpression doiRe(
        QStringLiteral(R"(10\.\d{4,9}/[-._;()/:A-Z0-9]+)"),
        QRegularExpression::CaseInsensitiveOption);
    auto doiMatches = doiRe.globalMatch(text);
    while (doiMatches.hasNext() && result.dois.size() < 20) {
        appendUnique(result.dois, cleanDoi(doiMatches.next().captured(0)));
    }

    const QRegularExpression temperatureRe(
        QStringLiteral(R"((?<!\d)(\d{2,4}(?:\.\d+)?)\s*(?:°\s*C|℃|deg\.?\s*C))"),
        QRegularExpression::CaseInsensitiveOption);
    auto temperatureMatches = temperatureRe.globalMatch(text);
    while (temperatureMatches.hasNext() && result.temperaturesC.size() < 50) {
        bool ok = false;
        const double value = temperatureMatches.next().captured(1).toDouble(&ok);
        if (ok) appendUnique(result.temperaturesC, value);
    }
    std::sort(result.temperaturesC.begin(), result.temperaturesC.end());

    const QRegularExpression timeRe(
        QStringLiteral(R"((?<![\w.])(\d+(?:\.\d+)?)\s*(h|hr|hrs|hour|hours|min|mins|minute|minutes)(?!\w))"),
        QRegularExpression::CaseInsensitiveOption);
    auto timeMatches = timeRe.globalMatch(text);
    while (timeMatches.hasNext() && result.durationsHours.size() < 100) {
        const auto match = timeMatches.next();
        bool ok = false;
        double value = match.captured(1).toDouble(&ok);
        if (!ok) continue;
        const QString unit = match.captured(2).toCaseFolded();
        if (!unit.startsWith(QLatin1Char('h'))) value /= 60.0;
        appendUnique(result.durationsHours, value);
    }
    std::sort(result.durationsHours.begin(), result.durationsHours.end());

    const QRegularExpression conversionRe(
        QStringLiteral(R"((?:CH\s*4|CH4|methane)[^\n%]{0,80}?(\d+(?:\.\d+)?)\s*%)"),
        QRegularExpression::CaseInsensitiveOption);
    auto conversionMatches = conversionRe.globalMatch(text);
    while (conversionMatches.hasNext() && result.ch4ConversionPercentCandidates.size() < 100) {
        bool ok = false;
        const double value = conversionMatches.next().captured(1).toDouble(&ok);
        if (ok) result.ch4ConversionPercentCandidates.append(value);
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
            if (folded.contains(term.toCaseFolded())) detected.append(term);
        }
        if (!detected.isEmpty()) result.keywordEvidence.insert(it.key(), detected);
    }

    result.snippets = evidenceSnippetsWithPages(
        text, defaultEvidenceTerms(), result.pageStartOffsets, 140, 12);
    return result;
}

QVector<EvidenceItem> DocumentAnalyzer::candidateItems(
    const QString& text,
    const DocumentSignals& document) {
    QVector<EvidenceItem> items;

    for (const QString& doi : document.dois) {
        const qsizetype index = text.indexOf(doi, 0, Qt::CaseInsensitive);
        addCandidate(items, document, QStringLiteral("doi"), QStringLiteral("DOI"), doi,
            contextAroundIndex(text, index, doi.size()), pageForIndex(document.pageStartOffsets, index));
    }
    for (double value : document.temperaturesC) {
        const QString rendered = QString::number(value, 'g', 10);
        const qsizetype index = text.indexOf(rendered, 0, Qt::CaseInsensitive);
        addCandidate(items, document, QStringLiteral("temperature_c"), QStringLiteral("temperature"), rendered,
            contextAroundIndex(text, index, rendered.size()), pageForIndex(document.pageStartOffsets, index));
    }
    for (double value : document.durationsHours) {
        const QString rendered = QString::number(value, 'g', 10);
        const qsizetype index = text.indexOf(rendered, 0, Qt::CaseInsensitive);
        addCandidate(items, document, QStringLiteral("duration_h"), QStringLiteral("duration"), rendered,
            contextAroundIndex(text, index, rendered.size()), pageForIndex(document.pageStartOffsets, index));
    }
    for (double value : document.ch4ConversionPercentCandidates) {
        const QString rendered = QString::number(value, 'g', 10);
        const qsizetype index = text.indexOf(rendered, 0, Qt::CaseInsensitive);
        addCandidate(items, document, QStringLiteral("ch4_conversion_percent_candidate"),
            QStringLiteral("CH4 conversion"), rendered,
            contextAroundIndex(text, index, rendered.size()), pageForIndex(document.pageStartOffsets, index));
    }
    for (const auto& snippet : document.snippets) {
        addCandidate(items, document, QStringLiteral("keyword_evidence"), snippet.term,
            snippet.term, snippet.snippet, snippet.sourcePage);
    }
    return items;
}

QVector<EvidenceSnippet> DocumentAnalyzer::evidenceSnippets(
    const QString& text,
    const QStringList& terms,
    int radius,
    int limit) {
    return evidenceSnippetsWithPages(text, terms, {0}, radius, limit);
}

QStringList DocumentAnalyzer::defaultEvidenceTerms() {
    return {
        QStringLiteral("stability"), QStringLiteral("deactivation"), QStringLiteral("time-on-stream"),
        QStringLiteral("coke"), QStringLiteral("coking"), QStringLiteral("sintering"),
        QStringLiteral("regeneration"), QStringLiteral("稳定"), QStringLiteral("失活"),
        QStringLiteral("积碳"), QStringLiteral("烧结"), QStringLiteral("再生")
    };
}

QString DocumentAnalyzer::bindingStatus(
    const QString& catalyst,
    const std::optional<double>& timeHours) {
    if (catalyst.trimmed().isEmpty()) {
        return QStringLiteral("candidate_requires_condition_binding");
    }
    if (!timeHours.has_value()) {
        return QStringLiteral("bound_to_catalyst_requires_time_condition_review");
    }
    return QStringLiteral("bound_to_catalyst_time_requires_condition_review");
}

} // namespace catalyst
