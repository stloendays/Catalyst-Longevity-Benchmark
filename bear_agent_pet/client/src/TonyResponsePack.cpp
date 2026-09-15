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

PackCache &cache() {
    static PackCache value;
    return value;
}

QString defaultPath() {
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("assets/config/tony_responses.json"));
}

QString configuredPath() {
    const QString custom=QSettings().value(QStringLiteral("tony/response_pack_path")).toString().trimmed();
    return custom.isEmpty() ? defaultPath() : custom;
}

void ensureLoaded() {
    auto &c=cache();
    const QString path=configuredPath();
    const QFileInfo info(path);
    const QDateTime modified=info.exists() ? info.lastModified() : QDateTime();

    if(c.path==path && c.modified==modified) return;

    c.path=path;
    c.modified=modified;
    c.root={};
    c.loaded=false;

    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError error;
    const auto document=QJsonDocument::fromJson(file.readAll(),&error);
    if(error.error!=QJsonParseError::NoError || !document.isObject()) return;

    const auto root=document.object();
    if(root.value(QStringLiteral("schema")).toInt()!=1) return;
    if(!root.value(QStringLiteral("locales")).isObject()) return;

    c.root=root;
    c.loaded=true;
}

QStringList replies(const QString &intent,const QString &language) {
    ensureLoaded();
    const auto &c=cache();
    if(!c.loaded) return {};

    const QString locale=language.trimmed().toLower().startsWith(QStringLiteral("zh"))
        ? QStringLiteral("zh") : QStringLiteral("en");
    const auto locales=c.root.value(QStringLiteral("locales")).toObject();
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
    ensureLoaded();
    return cache().path;
}

bool TonyResponsePack::loaded() {
    ensureLoaded();
    return cache().loaded;
}
