#include "TonyResponsePack.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSettings>

namespace {
struct PackCache {
    QString path;
    QDateTime modified;
    QJsonObject root;
    bool loaded{false};
};

PackCache &primaryCache() {
    static PackCache value;
    return value;
}

PackCache &autonomousCache() {
    static PackCache value;
    return value;
}

QString defaultPrimaryPath() {
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("assets/config/tony_responses.json"));
}

QString defaultAutonomousPath() {
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("assets/config/tony_autonomous_responses.json"));
}

QString configuredPrimaryPath() {
    const QString custom=QSettings().value(QStringLiteral("tony/response_pack_path")).toString().trimmed();
    return custom.isEmpty() ? defaultPrimaryPath() : custom;
}

QString configuredAutonomousPath() {
    const QString custom=QSettings().value(QStringLiteral("tony/autonomous_response_pack_path")).toString().trimmed();
    return custom.isEmpty() ? defaultAutonomousPath() : custom;
}

void ensureLoaded(PackCache &cache,const QString &path) {
    const QFileInfo info(path);
    const QDateTime modified=info.exists() ? info.lastModified() : QDateTime();
    if(cache.path==path && cache.modified==modified) return;

    cache.path=path;
    cache.modified=modified;
    cache.root={};
    cache.loaded=false;

    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError error;
    const auto document=QJsonDocument::fromJson(file.readAll(),&error);
    if(error.error!=QJsonParseError::NoError || !document.isObject()) return;

    const auto root=document.object();
    if(root.value(QStringLiteral("schema")).toInt()!=1) return;
    if(!root.value(QStringLiteral("locales")).isObject()) return;

    cache.root=root;
    cache.loaded=true;
}

QStringList repliesFrom(const PackCache &cache,const QString &intent,const QString &language) {
    if(!cache.loaded) return {};

    const QString locale=language.trimmed().toLower().startsWith(QStringLiteral("zh"))
        ? QStringLiteral("zh") : QStringLiteral("en");
    const auto locales=cache.root.value(QStringLiteral("locales")).toObject();
    const auto localeObject=locales.value(locale).toObject();
    const auto value=localeObject.value(intent);
    if(!value.isArray()) return {};

    QStringList out;
    for(const auto &item:value.toArray()) {
        const QString text=item.toString().trimmed();
        if(!text.isEmpty()) out.push_back(text);
    }
    return out;
}

QStringList replies(const QString &intent,const QString &language) {
    auto &primary=primaryCache();
    auto &autonomous=autonomousCache();
    ensureLoaded(primary,configuredPrimaryPath());
    ensureLoaded(autonomous,configuredAutonomousPath());

    const auto packedPrimary=repliesFrom(primary,intent,language);
    if(!packedPrimary.isEmpty()) return packedPrimary;
    return repliesFrom(autonomous,intent,language);
}

QString choose(const QStringList &items) {
    if(items.isEmpty()) return {};
    return items.at(QRandomGenerator::global()->bounded(items.size()));
}
}

QString TonyResponsePack::pick(const QString &intent,
                               const QString &language,
                               const QStringList &fallbackEnglish,
                               const QStringList &fallbackChinese) {
    const auto packed=replies(intent,language);
    if(!packed.isEmpty()) return choose(packed);

    const bool zh=language.trimmed().toLower().startsWith(QStringLiteral("zh"));
    return choose(zh ? fallbackChinese : fallbackEnglish);
}

QString TonyResponsePack::sourcePath() {
    auto &primary=primaryCache();
    ensureLoaded(primary,configuredPrimaryPath());
    return primary.path;
}

bool TonyResponsePack::loaded() {
    auto &primary=primaryCache();
    ensureLoaded(primary,configuredPrimaryPath());
    return primary.loaded;
}
