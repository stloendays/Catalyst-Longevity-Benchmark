#include "reportexporter.h"

#include "analysisengine.h"
#include "conditionguard.h"
#include "evidencepacket.h"

#include <QDateTime>
#include <QFileInfo>
#include <QFont>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>

namespace catalyst {

namespace {

QString escape(const QString& value) { return value.toHtmlEscaped(); }

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
    for (const auto& field : fields) labels.append(fieldLabel(field));
    return labels.join(QStringLiteral("、"));
}

QString evidenceCategoryLabel(const QString& category) {
    if (category == QStringLiteral("doi")) return QStringLiteral("DOI");
    if (category == QStringLiteral("temperature_c")) return QStringLiteral("温度");
    if (category == QStringLiteral("duration_h")) return QStringLiteral("测试时长");
    if (category == QStringLiteral("ch4_conversion_percent_candidate")) return QStringLiteral("CH4 转化率候选");
    if (category == QStringLiteral("keyword_evidence")) return QStringLiteral("失活/稳定证据词");
    return category;
}

QString evidenceStatusLabel(const QString& status) {
    if (status == QStringLiteral("candidate_requires_condition_binding")) return QStringLiteral("候选：待绑定");
    if (status == QStringLiteral("bound_to_catalyst_requires_time_condition_review")) return QStringLiteral("已绑定催化剂：待时间/条件复核");
    if (status == QStringLiteral("bound_to_catalyst_time_requires_condition_review")) return QStringLiteral("已绑定催化剂+时间：待条件复核");
    if (status == QStringLiteral("condition_reviewed_context_only")) return QStringLiteral("条件已人工复核：仅作上下文");
    return status;
}

QString truncate(QString value, int maxLength = 260) {
    value = value.simplified();
    if (value.size() <= maxLength) return value;
    return value.left(maxLength - 1) + QChar(0x2026);
}

QString sourcePageText(const EvidenceItem& item) {
    if (item.sourcePage <= 0) return QStringLiteral("—");
    return QStringLiteral("p.%1").arg(item.sourcePage);
}

QString buildHtml(
    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems) {
    const EvidencePacket packet = EvidencePacketBuilder::build(evidenceItems);

    QString html;
    html += QStringLiteral(
        "<html><head><meta charset='utf-8'>"
        "<style>"
        "body{font-family:'Microsoft YaHei UI','Segoe UI',sans-serif;color:#17202a;font-size:9.5pt;}"
        "h1{font-size:21pt;margin-bottom:4px;} h2{font-size:14pt;margin-top:20px;} h3{font-size:11.5pt;margin-top:14px;}"
        "p.meta{color:#5f6b76;}"
        "table{border-collapse:collapse;width:100%;margin-top:8px;}"
        "th,td{border:1px solid #d8dde3;padding:5px 6px;text-align:left;vertical-align:top;}"
        "th{background:#f1f3f5;} .warn{color:#a61b1b;font-weight:600;}"
        ".ok{color:#166534;font-weight:600;} .note{background:#eef6ff;padding:10px;}"
        ".small{font-size:8.5pt;color:#4b5563;}"
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
        html += QStringLiteral("<p>已审计字段：%1</p>").arg(escape(fieldList(result.conditionAudit.explicitFields)));
    }
    if (!result.conditionAudit.pairMismatches.isEmpty()) {
        html += QStringLiteral("<table><tr><th>催化剂 A</th><th>催化剂 B</th><th>不匹配条件</th></tr>");
        for (const auto& mismatch : result.conditionAudit.pairMismatches) {
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                .arg(escape(mismatch.catalystA), escape(mismatch.catalystB), escape(fieldList(mismatch.fields)));
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

    html += QStringLiteral("<h2>Evidence Packet</h2>");
    html += QStringLiteral(
        "<table><tr><th>AI 可用</th><th>已复核上下文</th><th>待复核/排除</th><th>来源</th><th>催化剂</th></tr>");
    html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr></table>")
        .arg(packet.readyForAi ? QStringLiteral("是") : QStringLiteral("否"))
        .arg(packet.reviewedContextItems)
        .arg(packet.pendingItems)
        .arg(packet.sourceCount)
        .arg(packet.catalystCount);

    if (!packet.warnings.isEmpty()) {
        html += QStringLiteral("<p class='note'><b>Packet guardrails：</b><br/>");
        for (const auto& warning : packet.warnings) {
            html += QStringLiteral("• %1<br/>").arg(escape(warning));
        }
        html += QStringLiteral("</p>");
    }

    if (!packet.contextItems.isEmpty()) {
        html += QStringLiteral(
            "<table><tr><th>来源/页码</th><th>绑定</th><th>类别</th><th>候选</th><th>复核备注</th><th>原文上下文</th></tr>");
        for (const auto& item : packet.contextItems) {
            const QString source = item.sourcePath.isEmpty()
                ? QStringLiteral("—")
                : QFileInfo(item.sourcePath).fileName();
            const QString sourceWithPage = QStringLiteral("%1 · %2").arg(source, sourcePageText(item));
            const QString binding = QStringLiteral("%1 @ %2 h")
                .arg(item.boundCatalyst, QString::number(*item.boundTimeHours, 'g', 8));
            const QString candidate = item.valueText.isEmpty() ? item.term : item.valueText;
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td class='small'>%5</td><td class='small'>%6</td></tr>")
                .arg(escape(sourceWithPage),
                     escape(binding),
                     escape(evidenceCategoryLabel(item.category)),
                     escape(candidate),
                     escape(truncate(item.note)),
                     escape(truncate(item.snippet)));
        }
        html += QStringLiteral("</table>");
    }

    html += QStringLiteral(
        "<p class='note'><b>Evidence Packet 语义：</b>只有完成催化剂、时间和人工条件复核的条目进入 AI 上下文。"
        "Packet 为 context-only，不得覆盖实验观测、寿命阈值或直接排名。</p>");

    html += QStringLiteral("<h2>资料证据审计附录</h2>");
    if (evidenceItems.isEmpty()) {
        html += QStringLiteral("<p>当前项目未保存资料证据候选。</p>");
    } else {
        int unbound = 0;
        int boundPending = 0;
        int reviewed = 0;
        for (const auto& item : evidenceItems) {
            if (item.status == QStringLiteral("condition_reviewed_context_only")) ++reviewed;
            else if (item.boundCatalyst.isEmpty()) ++unbound;
            else ++boundPending;
        }
        html += QStringLiteral("<table><tr><th>证据候选</th><th>未绑定</th><th>已绑定待复核</th><th>条件已人工复核</th></tr>");
        html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr></table>")
            .arg(evidenceItems.size()).arg(unbound).arg(boundPending).arg(reviewed);
        html += QStringLiteral(
            "<p class='note'><b>重要：</b>“条件已人工复核”只表示用户完成了资料上下文核对。"
            "这些资料证据仍不会自动修改实验观测、寿命阈值或排名。</p>");

        html += QStringLiteral(
            "<table><tr><th>来源</th><th>页</th><th>类别</th><th>候选</th><th>状态</th><th>绑定</th><th>备注 / 原文</th></tr>");
        for (const auto& item : evidenceItems) {
            const QString source = item.sourcePath.isEmpty() ? QStringLiteral("—") : QFileInfo(item.sourcePath).fileName();
            const QString binding = item.boundCatalyst.isEmpty()
                ? QStringLiteral("—")
                : (item.boundTimeHours.has_value()
                    ? QStringLiteral("%1 @ %2 h").arg(item.boundCatalyst, QString::number(*item.boundTimeHours, 'g', 8))
                    : item.boundCatalyst);
            QString context = item.note;
            if (!item.snippet.isEmpty()) {
                context = context.isEmpty() ? item.snippet : QStringLiteral("%1 | 原文：%2").arg(context, item.snippet);
            }
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td><td class='small'>%7</td></tr>")
                .arg(escape(source),
                     escape(sourcePageText(item)),
                     escape(evidenceCategoryLabel(item.category)),
                     escape(item.valueText.isEmpty() ? item.term : item.valueText),
                     escape(evidenceStatusLabel(item.status)),
                     escape(binding),
                     escape(truncate(context)));
        }
        html += QStringLiteral("</table>");
    }

    html += QStringLiteral(
        "<p class='note'><b>证据语义：</b> T95 / T90 / T80 采用离散观测的删失语义。"
        "报告不会把两个实际观测点之间的插值结果冒充为直接测得的精确寿命。"
        "PDF 页码、来源 SHA-256、绑定与人工复核状态用于可追溯性；资料证据与实验观测在项目文件中分开存储。</p>");
    html += QStringLiteral("</body></html>");
    return html;
}

bool writePdf(
    const QString& path,
    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems,
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
    document.setHtml(buildHtml(result, sourceLabel, evidenceItems));
    document.print(&writer);

    const QFileInfo output(path);
    if (!output.exists() || output.size() <= 0) {
        if (errorMessage) *errorMessage = QStringLiteral("PDF 报告生成失败。");
        return false;
    }
    if (errorMessage) *errorMessage = QStringLiteral("PDF 报告已导出：%1").arg(path);
    return true;
}

} // namespace

bool ReportExporter::exportPdf(
    const QString& path,
    const AnalysisResult& result,
    const QString& sourceLabel,
    QString* errorMessage) {
    return writePdf(path, result, sourceLabel, {}, errorMessage);
}

bool ReportExporter::exportPdf(
    const QString& path,
    const AnalysisResult& result,
    const QString& sourceLabel,
    const QVector<EvidenceItem>& evidenceItems,
    QString* errorMessage) {
    return writePdf(path, result, sourceLabel, evidenceItems, errorMessage);
}

} // namespace catalyst
