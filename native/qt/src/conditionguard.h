#pragma once

#include "models.h"

#include <QString>
#include <QVector>

namespace catalyst {

class ConditionGuard {
public:
    static ConditionAudit audit(const QVector<Record>& records);
    static QString statusText(ConditionAuditStatus status);
};

} // namespace catalyst
