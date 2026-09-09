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
            "当前没有完成‘催化剂 + 时间 + 确认备注’的资料，因此暂不提供给 AI。"));
    }
    if (packet.pendingItems > 0) {
        packet.warnings.append(QStringLiteral(
            "%1 条资料仍待关联或确认，当前不会提供给 AI。")
            .arg(packet.pendingItems));
    }
    if (reviewedButIncomplete > 0) {
        packet.warnings.append(QStringLiteral(
            "%1 条资料虽然标记为已确认，但关联信息不完整，已自动退回待确认状态。")
            .arg(reviewedButIncomplete));
    }
    if (packet.sourceCount == 0 && packet.totalCandidates > 0) {
        packet.warnings.append(QStringLiteral("部分资料缺少可追溯来源标识。"));
    }

    return packet;
}

QString EvidencePacketBuilder::toMarkdown(const EvidencePacket& packet) {
    QString text;
    text += QStringLiteral("# AI 可用资料\n\n");
    text += QStringLiteral("- 生成时间：%1\n").arg(packet.generatedUtc);
    text += QStringLiteral("- AI 可用：%1\n").arg(packet.readyForAi ? QStringLiteral("是") : QStringLiteral("否"));
    text += QStringLiteral("- 已确认资料：%1 条\n").arg(packet.reviewedContextItems);
    text += QStringLiteral("- 待确认资料：%1 条\n").arg(packet.pendingItems);
    text += QStringLiteral("- 来源数量：%1\n").arg(packet.sourceCount);
    text += QStringLiteral("- 关联催化剂：%1 个\n\n").arg(packet.catalystCount);

    if (!packet.warnings.isEmpty()) {
        text += QStringLiteral("## 使用提示\n\n");
        for (const auto& warning : packet.warnings) {
            text += QStringLiteral("- %1\n").arg(warning);
        }
        text += QLatin1Char('\n');
    }

    text += QStringLiteral("## 已确认资料\n\n");
    if (packet.contextItems.isEmpty()) {
        text += QStringLiteral("当前没有可供 AI 使用的已确认资料。\n");
        return text;
    }

    int index = 1;
    for (const auto& item : packet.contextItems) {
        const QString source = item.sourcePath.isEmpty()
            ? QStringLiteral("未知来源")
            : QFileInfo(item.sourcePath).fileName();
        const QString page = item.sourcePage > 0
            ? QStringLiteral("第 %1 页").arg(item.sourcePage)
            : QStringLiteral("页码未知");
        const QString candidate = item.valueText.isEmpty() ? item.term : item.valueText;
        const QString hash = item.sourceSha256.isEmpty()
            ? QStringLiteral("未知")
            : item.sourceSha256.left(16);

        text += QStringLiteral("### 资料 %1\n\n").arg(index++);
        text += QStringLiteral("- 来源：%1\n").arg(source);
        text += QStringLiteral("- 来源校验：%1\n").arg(hash);
        text += QStringLiteral("- 页码：%1\n").arg(page);
        text += QStringLiteral("- 催化剂：%1\n").arg(item.boundCatalyst);
        text += QStringLiteral("- 时间：%1 h\n").arg(QString::number(*item.boundTimeHours, 'g', 10));
        text += QStringLiteral("- 类型：%1\n").arg(categoryLabel(item.category));
        text += QStringLiteral("- 识别内容：%1\n").arg(candidate);
        text += QStringLiteral("- 确认备注：%1\n").arg(compact(item.note));
        if (!item.snippet.trimmed().isEmpty()) {
            text += QStringLiteral("- 原文上下文：%1\n").arg(compact(item.snippet));
        }
        text += QLatin1Char('\n');
    }

    text += QStringLiteral("以上资料仅作为分析上下文，不会改写实验观测、寿命阈值或催化剂直接排名。\n");
    return text;
}

} // namespace catalyst
