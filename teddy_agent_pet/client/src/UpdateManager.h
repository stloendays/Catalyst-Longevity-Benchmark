#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QWidget;

class UpdateManager final : public QObject {
public:
    explicit UpdateManager(QWidget *parentWidget=nullptr);

    void scheduleAutomaticCheck();
    void checkForUpdates(bool manual=false);

private:
    struct ReleaseInfo {
        QString version;
        QString notes;
        QUrl zipUrl;
        QUrl shaUrl;
    };

    QWidget *parentWidget_{nullptr};
    bool busy_{false};

    void cleanupOwnedUpdateArtifacts();
    void handleReleaseList(const QByteArray &data, bool manual);
    void fetchChecksumAndPrompt(const ReleaseInfo &release, bool manual);
    void promptAndDownload(const ReleaseInfo &release, const QString &sha256);
    void downloadAndApply(const ReleaseInfo &release, const QString &sha256);
    bool stageApplyScript(const QString &zipPath, const QString &targetVersion, QString *error);
    QString cacheDirectory() const;
    static bool isValidSha256(const QString &value);
};
