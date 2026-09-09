from pathlib import Path


def replace_once(path, old, new, label):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if old not in text:
        raise RuntimeError(f"missing {label} in {path}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# Public overload: raw records are needed for data checks, shared-time pairs and experiment advice.
replace_once(
    "native/qt/src/reportexporter.h",
    '''    static bool exportPdf(
        const QString& path,
        const AnalysisResult& result,
        const QString& sourceLabel,
        const QVector<EvidenceItem>& evidenceItems,
        QString* errorMessage = nullptr);''',
    '''    static bool exportPdf(
        const QString& path,
        const AnalysisResult& result,
        const QString& sourceLabel,
        const QVector<EvidenceItem>& evidenceItems,
        QString* errorMessage = nullptr);

    static bool exportPdf(
        const QString& path,
        const QVector<Record>& records,
        const AnalysisResult& result,
        const QString& sourceLabel,
        const QVector<EvidenceItem>& evidenceItems,
        QString* errorMessage = nullptr);''',
    "report overload",
)

p = Path("native/qt/src/reportexporter.cpp")
text = p.read_text(encoding="utf-8")
text = text.replace('#include "evidencepacket.h"\n', '#include "evidencepacket.h"\n#include "researchadvisor.h"\n', 1)
text = text.replace(
    '''QString buildHtml(
    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems) {''',
    '''QString buildHtml(
    const QVector<Record>& records,
    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems) {''',
    1,
)

insert_after = '''    } else {
        html += QStringLiteral("<p>当前数据不足以给出共同实际观测时间下的直接领先者。</p>");
    }

'''
if insert_after not in text:
    raise RuntimeError("direct comparison insertion point missing")
sections = r'''    const DataCheckResult dataCheck = ResearchAdvisor::checkData(records, result);
    html += QStringLiteral("<h2>数据检查</h2>");
    html += QStringLiteral(
        "<table><tr><th>完整度评分</th><th>需处理</th><th>建议</th><th>提示</th></tr>"
        "<tr><td>%1 / 100</td><td>%2</td><td>%3</td><td>%4</td></tr></table>")
        .arg(dataCheck.score)
        .arg(dataCheck.errorCount)
        .arg(dataCheck.warningCount)
        .arg(dataCheck.infoCount);
    if (!dataCheck.items.isEmpty()) {
        html += QStringLiteral("<table><tr><th>状态</th><th>范围</th><th>发现</th><th>建议</th></tr>");
        const qsizetype limit = qMin<qsizetype>(dataCheck.items.size(), 20);
        for (qsizetype i = 0; i < limit; ++i) {
            const auto& item = dataCheck.items[i];
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
                .arg(escape(ResearchAdvisor::checkLevelText(item.level)),
                     escape(item.scope), escape(item.issue), escape(item.suggestion));
        }
        html += QStringLiteral("</table>");
        if (dataCheck.items.size() > limit) {
            html += QStringLiteral("<p class='small'>报告仅展示前 %1 条，软件界面可查看全部检查结果。</p>").arg(limit);
        }
    }

    const auto pairComparisons = ResearchAdvisor::pairComparisons(records, result);
    html += QStringLiteral("<h2>同时间对比</h2>");
    if (pairComparisons.isEmpty()) {
        html += QStringLiteral("<p>当前没有可生成的催化剂两两对比。</p>");
    } else {
        html += QStringLiteral(
            "<table><tr><th>催化剂 A</th><th>催化剂 B</th><th>共同时间</th><th>A 性能</th>"
            "<th>B 性能</th><th>差值</th><th>状态</th><th>结果</th></tr>");
        for (const auto& comparison : pairComparisons) {
            const QString timeText = comparison.sharedTimeHours > 0.0
                ? QStringLiteral("%1 h").arg(QString::number(comparison.sharedTimeHours, 'g', 8))
                : QStringLiteral("—");
            const QString aText = comparison.sharedTimeHours > 0.0
                ? QString::number(comparison.performanceA, 'g', 8) : QStringLiteral("—");
            const QString bText = comparison.sharedTimeHours > 0.0
                ? QString::number(comparison.performanceB, 'g', 8) : QStringLiteral("—");
            const QString difference = comparison.sharedTimeHours > 0.0
                ? QString::number(comparison.absoluteDifference, 'g', 8) : QStringLiteral("—");
            const QString outcome = comparison.comparable && !comparison.leader.isEmpty()
                ? QStringLiteral("%1 当前较高").arg(comparison.leader)
                : QStringLiteral("暂不判断");
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td><td>%7</td><td>%8</td></tr>")
                .arg(escape(comparison.catalystA), escape(comparison.catalystB), escape(timeText),
                     escape(aText), escape(bText), escape(difference), escape(comparison.status), escape(outcome));
        }
        html += QStringLiteral("</table>");
    }

    const auto experimentAdvice = ResearchAdvisor::experimentAdvice(records, result);
    html += QStringLiteral("<h2>下一步实验建议</h2>");
    if (experimentAdvice.isEmpty()) {
        html += QStringLiteral("<p>当前数据未触发额外实验建议。</p>");
    } else {
        html += QStringLiteral("<table><tr><th>优先级</th><th>催化剂</th><th>建议</th><th>原因</th><th>下一步</th></tr>");
        for (const auto& advice : experimentAdvice) {
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                .arg(escape(ResearchAdvisor::advicePriorityText(advice.priority)),
                     escape(advice.catalyst), escape(advice.action), escape(advice.reason), escape(advice.target));
        }
        html += QStringLiteral("</table>");
    }
    html += QStringLiteral(
        "<p class='note'><b>实验建议说明：</b>上述建议由当前观测点、T90 状态、采样间隔和实验条件规则自动生成，"
        "用于辅助下一轮实验设计，不替代研究人员对具体反应体系的专业判断。</p>");

'''
text = text.replace(insert_after, insert_after + sections, 1)

text = text.replace(
    '''bool writePdf(
    const QString& path,
    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage) {''',
    '''bool writePdf(
    const QString& path,
    const QVector<Record>& records,
    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage) {''',
    1,
)
text = text.replace('document.setHtml(buildHtml(result, sourceLabel, evidenceItems));',
                    'document.setHtml(buildHtml(records, result, sourceLabel, evidenceItems));', 1)
text = text.replace('return writePdf(path, result, sourceLabel, {}, errorMessage);',
                    'return writePdf(path, {}, result, sourceLabel, {}, errorMessage);', 1)
text = text.replace('return writePdf(path, result, sourceLabel, evidenceItems, errorMessage);',
                    'return writePdf(path, {}, result, sourceLabel, evidenceItems, errorMessage);', 1)

end_marker = '''bool ReportExporter::exportPdf(
    const QString& path,
    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage) {
    return writePdf(path, {}, result, sourceLabel, evidenceItems, errorMessage);
}
'''
if end_marker not in text:
    raise RuntimeError("report implementation marker missing")
new_impl = end_marker + r'''
bool ReportExporter::exportPdf(
    const QString& path,
    const QVector<Record>& records,
    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage) {
    return writePdf(path, records, result, sourceLabel, evidenceItems, errorMessage);
}
'''
text = text.replace(end_marker, new_impl, 1)
p.write_text(text, encoding="utf-8")

# Main application export uses raw records so the PDF includes the practical modules.
replace_once(
    "native/qt/src/mainwindow.cpp",
    'if (!ReportExporter::exportPdf(path, analysis_, sourceLabelText_, evidence, &message)) {',
    'if (!ReportExporter::exportPdf(path, records_, analysis_, sourceLabelText_, evidence, &message)) {',
    "mainwindow report call",
)

# Self-test exercises the raw-record report overload.
replace_once(
    "native/qt/src/main.cpp",
    'if (!catalyst::ReportExporter::exportPdf(\n                reportPath, result, QStringLiteral("self-test"), loadedEvidence, &reportMessage)) return 24;',
    'if (!catalyst::ReportExporter::exportPdf(\n                reportPath, records, result, QStringLiteral("self-test"), loadedEvidence, &reportMessage)) return 24;',
    "self-test report call",
)

print("Decision support sections added to PDF export.")
