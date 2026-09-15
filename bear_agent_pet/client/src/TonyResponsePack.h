#pragma once

#include <QString>
#include <QStringList>

class TonyResponsePack {
public:
    static QString pick(const QString &intent,
                        const QString &language,
                        const QStringList &fallbackEnglish,
                        const QStringList &fallbackChinese);

    static QString sourcePath();
    static bool loaded();
};
