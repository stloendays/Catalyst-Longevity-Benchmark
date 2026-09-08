#include "evidencepacket.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSet>

namespace catalyst {

namespace {

QString sourceKey(const EvidenceItem& item) {
    if (!item.sourceSha256.trimmed().isEmpty()) return item.sourceSha256;
    return item.sourcePath;
}

QString compact(QString value, int maxLength = 420) {
    value = value.simplified();
    if (value.size() <= maxLength) return value;
    return value.left(maxLength - 1) + QChar(0x2026);
}

QString categoryLabel(const QString& category) {
    if (category == QStringLiteral("doi")) return QStringLiteral("DOI");
    if (category == QStringLiteral("temperature_c")) return QStringLiteral("温度");
    if (category == QStringLiteral("duration_h")) return QStringLiteral("测试时长");
    if (category == QStringLiteral("ch4_conversion_percent_candidate")) return QStringLiteral("CH4 转化率候选");
    if (category == QStringLiteral("keyword_evidence")) return QStringLiteral("失活/稳定证据词");
    return category;
}

} // namespace

EvidencePacket EvidencePacketBuilder::build(const QVector<EvidenceItem>& evidenceItems) {
    EvidencePacket packet;
    packet.generatedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    packet.totalCandidates = evidenceItems.size();

    QSet<QString> sources;
    QSet<QString> catalysts;
    int reviewedButIncomplete = 0;

    for (const auto& item : evidenceItems) {
        const QString source = sourceKey(item).trimmed();
        if (!source.isEmpty()) sources.insert(source);

        const bool markedReviewed = item.status == QStringLiteral("condition_reviewed_context_only");
        const bool completeBinding = !item.boundCatalyst.trimmed().isEmpty()
            && item.boundTimeHours.has_value()
            && !item.note.trimmed().isEmpty();

        if (markedReviewed && completeBinding) {
            packet.contextItems.append(item);
            catalysts.insert(item.boundCatalyst.trimmed());
        } else {
            ++packet.pendingItems;
            if (markedReviewed && !completeBinding) ++reviewedButIncomplete;
        }
    }

    packet.reviewedContextItems = packet.contextItems.size();
    packet.sourceCount = sources.size();
    packet.catalystCount = catalysts.size();
    packet.readyForAi = !packet.contextItems.isEmpty();

    if (!packet.readyForAi) {
        packet.warnings.append(QStringLiteral(
            "当前没有完成‘催化剂 + 时间 + 条件复核备注’的证据，因此 Evidence Packet 暂不进入 AI 上下文。"));
    }
    if (packet.pendingItems > 0) {
        packet.warnings.append(QStringLiteral(
            "%1 条证据仍处于候选/待复核状态，已从 AI 上下文中排除。")
            .arg(packet.pendingItems));
    }
    if (reviewedButIncomplete > 0) {
        packet.warnings.append(QStringLiteral(
            "%1 条证据虽然带有复核状态，但绑定信息不完整，已按安全策略降级为待复核。")
            .arg(reviewedButIncomplete));
    }
    if (packet.sourceCount == 0 && packet.totalCandidates > 0) {
        packet.warnings.append(QStringLiteral("部分证据缺少可追溯来源标识。"));
    }

    return packet;
}

QString EvidencePacketBuilder::toMarkdown(const EvidencePacket& packet) {
    QString text;
    text += QStringLiteral("# Evidence Packet\n\n");
    text += QStringLiteral("- generated_utc: %1\n").arg(packet.generatedUtc);
    text += QStringLiteral("- ready_for_ai: %1\n").arg(packet.readyForAi ? QStringLiteral("true") : QStringLiteral("false"));
    text += QStringLiteral("- reviewed_context_items: %1\n").arg(packet.reviewedContextItems);
    text += QStringLiteral("- pending_excluded_items: %1\n").arg(packet.pendingItems);
    text += QStringLiteral("- sources: %1\n").arg(packet.sourceCount);
    text += QStringLiteral("- catalysts: %1\n\n").arg(packet.catalystCount);

    if (!packet.warnings.isEmpty()) {
        text += QStringLiteral("## Guardrails\n\n");
        for (const auto& warning : packet.warnings) {
            text += QStringLiteral("- %1\n").arg(warning);
        }
        text += QLatin1Char('\n');
    }

    text += QStringLiteral("## Reviewed context\n\n");
    if (packet.contextItems.isEmpty()) {
        text += QStringLiteral("No reviewed evidence is currently eligible for AI context.\n");
        return text;
    }

    int index = 1;
    for (const auto& item : packet.contextItems) {
        const QString source = item.sourcePath.isEmpty()
            ? QStringLiteral("unknown")
            : QFileInfo(item.sourcePath).fileName();
        const QString page = item.sourcePage > 0
            ? QStringLiteral("p.%1").arg(item.sourcePage)
            : QStringLiteral("page unknown");
        const QString candidate = item.valueText.isEmpty() ? item.term : item.valueText;
        const QString hash = item.sourceSha256.isEmpty()
            ? QStringLiteral("unknown")
            : item.sourceSha256.left(16);

        text += QStringLiteral("### Evidence %1\n\n").arg(index++);
        text += QStringLiteral("- source: %1\n").arg(source);
        text += QStringLiteral("- source_sha256_prefix: %1\n").arg(hash);
        text += QStringLiteral("- page: %1\n").arg(page);
        text += QStringLiteral("- catalyst: %1\n").arg(item.boundCatalyst);
        text += QStringLiteral("- time_h: %1\n").arg(QString::number(*item.boundTimeHours, 'g', 10));
        text += QStringLiteral("- category: %1\n").arg(categoryLabel(item.category));
        text += QStringLiteral("- candidate: %1\n").arg(candidate);
        text += QStringLiteral("- review_note: %1\n").arg(compact(item.note));
        if (!item.snippet.trimmed().isEmpty()) {
            text += QStringLiteral("- source_context: %1\n").arg(compact(item.snippet));
        }
        text += QLatin1Char('\n');
    }

    text += QStringLiteral(
        "Evidence Packet is context-only. It must not overwrite experimental observations, "
        "lifetime thresholds, or direct catalyst rankings.\n");
    return text;
}

} // namespace catalyst
