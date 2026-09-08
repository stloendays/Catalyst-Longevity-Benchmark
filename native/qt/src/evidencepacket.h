#pragma once

#include "documentanalyzer.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace catalyst {

struct EvidencePacket {
    QString generatedUtc;
    int totalCandidates = 0;
    int reviewedContextItems = 0;
    int pendingItems = 0;
    int sourceCount = 0;
    int catalystCount = 0;
    bool readyForAi = false;
    QVector<EvidenceItem> contextItems;
    QStringList warnings;
};

class EvidencePacketBuilder {
public:
    static EvidencePacket build(const QVector<EvidenceItem>& evidenceItems);
    static QString toMarkdown(const EvidencePacket& packet);
};

} // namespace catalyst
