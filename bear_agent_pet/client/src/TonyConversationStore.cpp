#include "TonyConversationStore.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace {
const QString kConversationKey = QStringLiteral("tony/conversation/v1");
constexpr int kMaxEntries = 60;
constexpr int kMaxTextChars = 1800;
}

QString TonyConversationStore::normalizedRole(const QString &role) {
    const QString value = role.trimmed().toLower();
    if(value == QStringLiteral("assistant")) return value;
    return QStringLiteral("user");
}

QString TonyConversationStore::normalizedText(const QString &text) {
    QString value = text.simplified();
    if(value.size() > kMaxTextChars)
        value = value.left(kMaxTextChars - 3) + QStringLiteral("...");
    return value;
}

QJsonArray TonyConversationStore::entries() const {
    const QByteArray raw = QSettings().value(kConversationKey).toByteArray();
    if(raw.isEmpty()) return {};

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if(!doc.isArray()) return {};

    QJsonArray out;
    for(const auto &value : doc.array()) {
        if(!value.isObject()) continue;
        const QJsonObject obj = value.toObject();
        const QString role = normalizedRole(obj.value(QStringLiteral("role")).toString());
        const QString text = normalizedText(obj.value(QStringLiteral("text")).toString());
        const QString at = obj.value(QStringLiteral("at")).toString();
        if(text.isEmpty()) continue;
        out.append(QJsonObject{
            {QStringLiteral("role"), role},
            {QStringLiteral("text"), text},
            {QStringLiteral("at"), at}
        });
    }

    while(out.size() > kMaxEntries)
        out.removeFirst();
    return out;
}

void TonyConversationStore::append(const QString &role, const QString &text) {
    const QString clean = normalizedText(text);
    if(clean.isEmpty()) return;

    QJsonArray rows = entries();
    rows.append(QJsonObject{
        {QStringLiteral("role"), normalizedRole(role)},
        {QStringLiteral("text"), clean},
        {QStringLiteral("at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}
    });

    while(rows.size() > kMaxEntries)
        rows.removeFirst();

    QSettings().setValue(
        kConversationKey,
        QJsonDocument(rows).toJson(QJsonDocument::Compact));
}

void TonyConversationStore::clear() {
    QSettings().remove(kConversationKey);
}
