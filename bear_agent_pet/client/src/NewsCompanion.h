#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QUrl>
#include <QVector>

class PetWindow;

class NewsCompanion final : public QObject {
    Q_OBJECT
public:
    struct Source {
        QString id;
        QString name;
        QString category;
        QUrl url;
        bool defaultEnabled{true};
    };

    explicit NewsCompanion(PetWindow *pet, QObject *parent=nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);
    int intervalMinutes() const;
    void setIntervalMinutes(int minutes);
    int quietStartHour() const;
    int quietEndHour() const;
    void setQuietHours(int startHour, int endHour);

    QVector<Source> sources() const;
    bool sourceEnabled(const QString &id) const;
    void setSourceEnabled(const QString &id, bool enabled);

    QUrl latestStoryUrl() const;
    QString latestStoryTitle() const;
    QString latestStorySource() const;

public slots:
    void fetchNow(bool userInitiated=true);
    void openLatestStory();

signals:
    void enabledChanged(bool enabled);
    void intervalChanged(int minutes);
    void headlineReady(const QString &title, const QString &source, const QUrl &url);
    void statusChanged(const QString &text);
    void latestStoryChanged(const QString &title, const QString &source, const QUrl &url);

private:
    struct Item {
        QString title;
        QString source;
        QString stableId;
        QUrl url;
    };

    void loadSources();
    void scheduleNext(int delayMinutes=-1);
    void fetchNextSource();
    QVector<Item> parseFeed(const QByteArray &raw, const Source &source) const;
    bool isSeen(const Item &item) const;
    void markSeen(const Item &item);
    QString itemKey(const Item &item) const;
    bool quietHours() const;

    PetWindow *pet_{nullptr};
    QNetworkAccessManager network_;
    QTimer timer_;
    QVector<Source> sources_;
    QVector<int> fetchQueue_;
    int fetchQueuePos_{0};
    bool fetching_{false};
    bool userInitiated_{false};
    QUrl latestUrl_;
    QString latestTitle_;
    QString latestSource_;
};
