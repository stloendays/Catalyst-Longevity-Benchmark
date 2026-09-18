#pragma once

#include <QJsonArray>
#include <QString>

class TonyConversationStore {
public:
    QJsonArray entries() const;
    void append(const QString &role, const QString &text);
    void clear();

private:
    static QString normalizedRole(const QString &role);
    static QString normalizedText(const QString &text);
};
