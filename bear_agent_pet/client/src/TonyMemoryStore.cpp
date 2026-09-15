#include "TonyMemoryStore.h"

#include <QSettings>

namespace {
constexpr int kMaxFacts=24;
constexpr int kMaxFactChars=180;
const auto kSettingsKey=QStringLiteral("tony/memory/explicit_facts");

QString normalizedFact(QString value) {
    value=value.trimmed();
    while(value.endsWith(QLatin1Char('.')) || value.endsWith(QLatin1Char('!')) || value.endsWith(QLatin1Char('?')) ||
          value.endsWith(QChar(0x3002)) || value.endsWith(QChar(0xFF01)) || value.endsWith(QChar(0xFF1F))) {
        value.chop(1);
        value=value.trimmed();
    }
    return value.left(kMaxFactChars);
}
}

TonyMemoryStore &TonyMemoryStore::instance() {
    static TonyMemoryStore store;
    return store;
}

QStringList TonyMemoryStore::load() const {
    QStringList out;
    for(const auto &value:QSettings().value(kSettingsKey).toStringList()) {
        const QString fact=normalizedFact(value);
        if(!fact.isEmpty()) out.push_back(fact);
    }
    return out;
}

void TonyMemoryStore::save(const QStringList &items) const {
    QStringList clean;
    for(const auto &value:items) {
        const QString fact=normalizedFact(value);
        if(fact.isEmpty()) continue;
        bool duplicate=false;
        for(const auto &existing:clean) {
            if(existing.compare(fact,Qt::CaseInsensitive)==0) { duplicate=true; break; }
        }
        if(!duplicate) clean.push_back(fact);
    }
    while(clean.size()>kMaxFacts) clean.removeFirst();
    QSettings().setValue(kSettingsKey,clean);
}

bool TonyMemoryStore::remember(const QString &fact) {
    const QString cleanFact=normalizedFact(fact);
    if(cleanFact.isEmpty()) return false;

    auto items=load();
    for(const auto &existing:items) {
        if(existing.compare(cleanFact,Qt::CaseInsensitive)==0) return true;
    }
    items.push_back(cleanFact);
    save(items);
    return true;
}

bool TonyMemoryStore::forgetMatching(const QString &query) {
    const QString needle=normalizedFact(query);
    if(needle.isEmpty()) return false;

    auto items=load();
    const int before=items.size();
    for(int i=items.size()-1;i>=0;--i) {
        if(items.at(i).contains(needle,Qt::CaseInsensitive) || needle.contains(items.at(i),Qt::CaseInsensitive))
            items.removeAt(i);
    }
    if(items.size()==before) return false;
    save(items);
    return true;
}

void TonyMemoryStore::clear() {
    QSettings().remove(kSettingsKey);
}

QStringList TonyMemoryStore::facts() const {
    return load();
}
