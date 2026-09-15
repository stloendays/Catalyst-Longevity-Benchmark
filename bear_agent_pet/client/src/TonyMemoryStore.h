#pragma once

#include <QString>
#include <QStringList>

class TonyMemoryStore {
public:
    static TonyMemoryStore &instance();

    bool remember(const QString &fact);
    bool forgetMatching(const QString &query);
    void clear();
    QStringList facts() const;

private:
    TonyMemoryStore()=default;

    QStringList load() const;
    void save(const QStringList &items) const;
};
