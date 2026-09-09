#include "conditionguard.h"

#include <QMap>
#include <QSet>
#include <algorithm>

namespace catalyst {

namespace {

using FieldValues = QMap<QString, QSet<QString>>;

QString normalizeNumber(double value) {
    return QString::number(value, 'g', 15);
}

QString normalizeFeedRatio(QString value) {
    value = value.trimmed().toCaseFolded();
    value.remove(QLatin1Char(' '));
    value.replace(QChar(0xFF1A), QLatin1Char(':'));
    return value;
}

void addNumeric(FieldValues& fields, const QString& name, const std::optional<double>& value) {
    if (value.has_value()) {
        fields[name].insert(normalizeNumber(*value));
    }
}

void addText(FieldValues& fields, const QString& name, const QString& value) {
    const QString normalized = normalizeFeedRatio(value);
    if (!normalized.isEmpty()) {
        fields[name].insert(normalized);
    }
}

QStringList sortedFields(const QSet<QString>& fields) {
    QStringList values(fields.cbegin(), fields.cend());
    values.sort(Qt::CaseInsensitive);
    return values;
}

} // namespace

ConditionAudit ConditionGuard::audit(const QVector<Record>& records) {
    ConditionAudit audit;
    if (records.isEmpty()) {
        audit.status = ConditionAuditStatus::NoData;
        audit.message = QStringLiteral("暂无可检查的数据。");
        return audit;
    }

    QMap<QString, FieldValues> byCatalyst;
    QSet<QString> explicitFields;

    for (const auto& record : records) {
        const QString catalystName = record.catalyst.trimmed();
        if (catalystName.isEmpty()) {
            continue;
        }

        auto& fields = byCatalyst[catalystName];

        if (record.temperatureC.has_value()) {
            explicitFields.insert(QStringLiteral("temperature_c"));
        }
        if (record.gHSV.has_value()) {
            explicitFields.insert(QStringLiteral("ghsv"));
        }
        if (record.wHSV.has_value()) {
            explicitFields.insert(QStringLiteral("whsv"));
        }
        if (record.pressure.has_value()) {
            explicitFields.insert(QStringLiteral("pressure_bar"));
        }
        if (!record.feedRatio.trimmed().isEmpty()) {
            explicitFields.insert(QStringLiteral("feed_ratio"));
        }

        addNumeric(fields, QStringLiteral("temperature_c"), record.temperatureC);
        addNumeric(fields, QStringLiteral("ghsv"), record.gHSV);
        addNumeric(fields, QStringLiteral("whsv"), record.wHSV);
        addNumeric(fields, QStringLiteral("pressure_bar"), record.pressure);
        addText(fields, QStringLiteral("feed_ratio"), record.feedRatio);
    }

    audit.explicitFields = sortedFields(explicitFields);
    if (audit.explicitFields.isEmpty()) {
        audit.status = ConditionAuditStatus::ConditionsNotProvided;
        audit.message = QStringLiteral(
            "未提供显式实验条件；可以做描述性比较，但关键选材结论建议补充温度、空速、压力和进料条件。");
        return audit;
    }

    const QStringList catalystNames = byCatalyst.keys();
    for (qsizetype i = 0; i < catalystNames.size(); ++i) {
        for (qsizetype j = i + 1; j < catalystNames.size(); ++j) {
            const QString& catalystA = catalystNames[i];
            const QString& catalystB = catalystNames[j];
            const auto& fieldsA = byCatalyst[catalystA];
            const auto& fieldsB = byCatalyst[catalystB];

            QStringList mismatchFields;
            for (const auto& field : audit.explicitFields) {
                const QSet<QString> valuesA = fieldsA.value(field);
                const QSet<QString> valuesB = fieldsB.value(field);

                const bool internallyAmbiguous = valuesA.size() > 1 || valuesB.size() > 1;
                const bool explicitMismatch =
                    valuesA.size() == 1 && valuesB.size() == 1 && *valuesA.cbegin() != *valuesB.cbegin();

                if (internallyAmbiguous || explicitMismatch) {
                    mismatchFields.append(field);
                }
            }

            if (!mismatchFields.isEmpty()) {
                PairConditionMismatch mismatch;
                mismatch.catalystA = catalystA;
                mismatch.catalystB = catalystB;
                mismatch.fields = mismatchFields;
                audit.pairMismatches.append(mismatch);
            }
        }
    }

    if (audit.pairMismatches.isEmpty()) {
        audit.status = ConditionAuditStatus::MatchedOnProvidedConditions;
        audit.message = QStringLiteral("已填写的实验条件一致。");
    } else {
        audit.status = ConditionAuditStatus::MismatchDetected;
        audit.message = QStringLiteral("发现实验条件不一致，暂不显示直接排名。");
    }

    return audit;
}

QString ConditionGuard::statusText(ConditionAuditStatus status) {
    switch (status) {
    case ConditionAuditStatus::NoData:
        return QStringLiteral("暂无数据");
    case ConditionAuditStatus::ConditionsNotProvided:
        return QStringLiteral("条件未填写");
    case ConditionAuditStatus::MatchedOnProvidedConditions:
        return QStringLiteral("条件一致");
    case ConditionAuditStatus::MismatchDetected:
        return QStringLiteral("条件不一致");
    }
    return QStringLiteral("未知");
}

} // namespace catalyst
