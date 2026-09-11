#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>

class UpdateManager : public QObject {
    Q_OBJECT
public:
    explicit UpdateManager(QObject *parent = nullptr);

    bool automaticUpdatesEnabled() const;
    void setAutomaticUpdatesEnabled(bool enabled);
    QString latestVersion() const;
    bool hasDownloadedUpdate() const;

    void scheduleStartupCheck();
    void checkNow(bool userInitiated = false);
    void downloadAvailableUpdate();
    void applyDownloadedUpdate();

signals:
    void statusChanged(const QString &text);
    void updateFound(const QString &version);
    void noUpdateAvailable(const QString &currentVersion);
    void downloadProgress(qint64 received, qint64 total);
    void updateDownloaded(const QString &version);
    void updateError(const QString &message);

private:
    bool isNewerVersion(const QString &candidate) const;
    QString currentVersion() const;

    QNetworkAccessManager network_;
    QString latestVersion_;
    QUrl packageUrl_;
    QByteArray expectedSha256_;
    QString downloadedPath_;
    bool checking_{false};
    bool downloading_{false};
    bool userInitiatedCheck_{false};
};
