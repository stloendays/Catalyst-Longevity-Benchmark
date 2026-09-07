#include "reportexporter.h"

#include "analysisengine.h"
#include "conditionguard.h"

#include <QDateTime>
#include <QFileInfo>
#include <QFont>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>

namespace catalyst {

namespace {

QString escape(const QString& value) {
    return value.toHtmlEscaped();
}

QString fieldLabel(const QString& field) {
    if (field == QStringLiteral("temperature_c")) return QStringLiteral("温度");
    if (field == QStringLiteral("ghsv")) return QStringLiteral("GHSV");
    if (field == QStringLiteral("whsv")) return QStringLiteral("WHSV");
    if (field == QStringLiteral("pressure_bar")) return QStringLiteral("压力");
    if (field == QStringLiteral("feed_ratio")) return QStringLiteral("进料比");
    return field;
}

QString fieldList(const QStringList& fields) {
    QStringList labels;
    for (const auto& field : fields) {
        labels.append(fieldLabel(field));
    }
    return labels.join(QStringLiteral("、"));
}

QString buildHtml(const AnalysisResult& result, const QString& sourceLabel) {
    QString html;
    html += QStringLiteral(
        "<html><head><meta charset='utf-8'>"
        "<style>"
        "body{font-family:'Microsoft YaHei UI','Segoe UI',sans-serif;color:#17202a;font-size:10pt;}"
        "h1{font-size:21pt;margin-bottom:4px;} h2{font-size:14pt;margin-top:20px;}"
        "p.meta{color:#5f6b76;}"
        "table{border-collapse:collapse;width:100%;margin-top:8px;}"
        "th,td{border:1px solid #d8dde3;padding:6px 7px;text-align:left;}"
        "th{background:#f1f3f5;} .warn{color:#a61b1b;font-weight:600;}"
        ".ok{color:#166534;font-weight:600;} .note{background:#eef6ff;padding:10px;}"
        "</style></head><body>");

    html += QStringLiteral("<h1>Catalyst Longevity Research</h1>");
    html += QStringLiteral("<p class='meta'>催化剂长期表现分析报告</p>");
    html += QStringLiteral("<p class='meta'>生成时间：%1<br/>数据源：%2</p>")
        .arg(escape(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))),
             escape(sourceLabel.isEmpty() ? QStringLiteral("未标记") : sourceLabel));

    html += QStringLiteral("<h2>分析概览</h2>");
    html += QStringLiteral("<table><tr><th>催化剂数量</th><th>观测点</th><th>最长测试</th><th>条件守门</th></tr>");
    html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3 h</td><td>%4</td></tr></table>")
        .arg(result.catalysts.size())
        .arg(result.totalObservations)
        .arg(QString::number(result.longestTestHours, 'g', 8))
        .arg(escape(ConditionGuard::statusText(result.conditionAudit.status)));

    html += QStringLiteral("<h2>催化剂寿命摘要</h2>");
    html += QStringLiteral(
        "<table><tr><th>催化剂</th><th>观测点</th><th>初始性能</th><th>最新性能</th>"
        "<th>保持率</th><th>T95</th><th>T90</th><th>T80</th></tr>");
    for (const auto& summary : result.catalysts) {
        html += QStringLiteral(
            "<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5%</td><td>%6</td><td>%7</td><td>%8</td></tr>")
            .arg(escape(summary.catalyst))
            .arg(summary.observations)
            .arg(QString::number(summary.initialPerformance, 'g', 8))
            .arg(QString::number(summary.latestPerformance, 'g', 8))
            .arg(QString::number(summary.retentionPercent, 'f', 1))
            .arg(escape(AnalysisEngine::thresholdText(summary.t95)))
            .arg(escape(AnalysisEngine::thresholdText(summary.t90)))
            .arg(escape(AnalysisEngine::thresholdText(summary.t80)));
    }
    html += QStringLiteral("</table>");

    html += QStringLiteral("<h2>实验条件守门</h2>");
    const bool blocked = result.conditionAudit.blocksDirectRanking();
    html += QStringLiteral("<p class='%1'>%2</p>")
        .arg(blocked ? QStringLiteral("warn") : QStringLiteral("ok"),
             escape(result.conditionAudit.message.isEmpty()
                 ? QStringLiteral("没有额外条件审计信息。")
                 : result.conditionAudit.message));

    if (!result.conditionAudit.explicitFields.isEmpty()) {
        html += QStringLiteral("<p>已审计字段：%1</p>")
            .arg(escape(fieldList(result.conditionAudit.explicitFields)));
    }

    if (!result.conditionAudit.pairMismatches.isEmpty()) {
        html += QStringLiteral("<table><tr><th>催化剂 A</th><th>催化剂 B</th><th>不匹配条件</th></tr>");
        for (const auto& mismatch : result.conditionAudit.pairMismatches) {
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                .arg(escape(mismatch.catalystA),
                     escape(mismatch.catalystB),
                     escape(fieldList(mismatch.fields)));
        }
        html += QStringLiteral("</table>");
    }

    html += QStringLiteral("<h2>直接比较状态</h2>");
    if (blocked) {
        html += QStringLiteral("<p class='warn'>由于显式实验条件不匹配，直接跨催化剂领先结论被阻止。</p>");
    } else if (result.latestSharedTimeHours.has_value() && !result.latestSharedLeader.isEmpty()) {
        html += QStringLiteral("<p>最新共同实际观测时间：%1 h；该时间点表现领先：<b>%2</b>。</p>")
            .arg(QString::number(*result.latestSharedTimeHours, 'g', 8), escape(result.latestSharedLeader));
    } else {
        html += QStringLiteral("<p>当前数据不足以给出共同实际观测时间下的直接领先者。</p>");
    }

    html += QStringLiteral(
        "<p class='note'><b>证据语义：</b> T95 / T90 / T80 采用离散观测的删失语义。"
        "报告不会把两个实际观测点之间的插值结果冒充为直接测得的精确寿命。"
        "条件守门只根据用户显式提供的实验条件阻止明显不可比的直接排名。</p>");

    html += QStringLiteral("</body></html>");
    return html;
}

} // namespace

bool ReportExporter::exportPdf(
    const QString& path,
    const AnalysisResult& result,
    const QString& sourceLabel,
    QString* errorMessage) {
    if (path.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("报告输出路径为空。");
        return false;
    }
    if (result.catalysts.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("没有可导出的分析结果。请先导入并分析数据。");
        return false;
    }

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setTitle(QStringLiteral("Catalyst Longevity Research Analysis Report"));
    writer.setCreator(QStringLiteral("Catalyst Longevity Research"));
    writer.setResolution(120);

    QTextDocument document;
    document.setDefaultFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    document.setDocumentMargin(24.0);
    document.setHtml(buildHtml(result, sourceLabel));
    document.print(&writer);

    const QFileInfo output(path);
    if (!output.exists() || output.size() <= 0) {
        if (errorMessage) *errorMessage = QStringLiteral("PDF 报告生成失败。");
        return false;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("PDF 报告已导出：%1").arg(path);
    }
    return true;
}

} // namespace catalyst
