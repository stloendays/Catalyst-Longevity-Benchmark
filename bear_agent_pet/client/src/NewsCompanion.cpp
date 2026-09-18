#include "NewsCompanion.h"

#include "AppLogger.h"
#include "PetWindow.h"

#include <algorithm>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QTime>
#include <QXmlStreamReader>

namespace {
QString configPath() {
    return QCoreApplication::applicationDirPath()
        + QStringLiteral("/assets/config/news_sources.json");
}

QNetworkRequest requestFor(const QUrl &url) {
    QNetworkRequest request(url);
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QByteArray("TonyDesktopPet/") + QCoreApplication::applicationVersion().toUtf8());
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(
        QNetworkRequest::CacheLoadControlAttribute,
        QNetworkRequest::AlwaysNetwork);
    return request;
}

QString cleanText(QString text) {
    text = text.simplified();
    if(text.size() > 320) text = text.left(317) + QStringLiteral("...");
    return text;
}
}

NewsCompanion::NewsCompanion(PetWindow *pet, QObject *parent)
    : QObject(parent), pet_(pet) {
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, [this]{ fetchNow(false); });

    loadSources();

    QSettings s;
    latestUrl_ = s.value(QStringLiteral("news/latest_url")).toUrl();
    latestTitle_ = s.value(QStringLiteral("news/latest_title")).toString();
    latestSource_ = s.value(QStringLiteral("news/latest_source")).toString();

    if(enabled()) {
        // Give Tony time to finish startup, pairing and update checks before
        // the first background headline request.
        timer_.start(90 * 1000);
    }
}

bool NewsCompanion::enabled() const {
    return QSettings().value(QStringLiteral("news/enabled"), false).toBool();
}

void NewsCompanion::setEnabled(bool value) {
    QSettings().setValue(QStringLiteral("news/enabled"), value);
    if(value) {
        timer_.start(2500);
        emit statusChanged(QStringLiteral("News Companion enabled."));
    } else {
        timer_.stop();
        emit statusChanged(QStringLiteral("News Companion paused."));
    }
    emit enabledChanged(value);
}

int NewsCompanion::intervalMinutes() const {
    const int raw = QSettings().value(QStringLiteral("news/interval_minutes"), 120).toInt();
    if(raw <= 60) return 60;
    if(raw <= 120) return 120;
    return 240;
}

void NewsCompanion::setIntervalMinutes(int minutes) {
    const int normalized = minutes <= 60 ? 60 : (minutes <= 120 ? 120 : 240);
    QSettings().setValue(QStringLiteral("news/interval_minutes"), normalized);
    emit intervalChanged(normalized);
    if(enabled()) scheduleNext(normalized);
}

QVector<NewsCompanion::Source> NewsCompanion::sources() const {
    return sources_;
}

bool NewsCompanion::sourceEnabled(const QString &id) const {
    for(const auto &source : sources_) {
        if(source.id != id) continue;
        return QSettings().value(
            QStringLiteral("news/source/%1").arg(id),
            source.defaultEnabled).toBool();
    }
    return false;
}

void NewsCompanion::setSourceEnabled(const QString &id, bool value) {
    QSettings().setValue(QStringLiteral("news/source/%1").arg(id), value);
}

QUrl NewsCompanion::latestStoryUrl() const { return latestUrl_; }
QString NewsCompanion::latestStoryTitle() const { return latestTitle_; }
QString NewsCompanion::latestStorySource() const { return latestSource_; }

void NewsCompanion::loadSources() {
    QFile file(configPath());
    if(file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonArray rows = doc.object().value(QStringLiteral("sources")).toArray();
        for(const auto &row : rows) {
            const auto obj = row.toObject();
            Source source;
            source.id = obj.value(QStringLiteral("id")).toString().trimmed();
            source.name = obj.value(QStringLiteral("name")).toString().trimmed();
            source.category = obj.value(QStringLiteral("category")).toString().trimmed();
            source.url = QUrl(obj.value(QStringLiteral("url")).toString().trimmed());
            source.defaultEnabled = obj.value(QStringLiteral("enabled_by_default")).toBool(true);
            if(source.id.isEmpty() || source.name.isEmpty() ||
               !source.url.isValid() || source.url.scheme().toLower() != QStringLiteral("https"))
                continue;
            sources_.push_back(source);
        }
    }

    if(!sources_.isEmpty()) return;

    // Safe built-in fallback if the packaged JSON is accidentally missing.
    sources_ = {
        {QStringLiteral("bbc_world"), QStringLiteral("BBC World"),
         QStringLiteral("world"), QUrl(QStringLiteral("https://feeds.bbci.co.uk/news/world/rss.xml")), true},
        {QStringLiteral("bbc_technology"), QStringLiteral("BBC Technology"),
         QStringLiteral("technology"), QUrl(QStringLiteral("https://feeds.bbci.co.uk/news/technology/rss.xml")), true},
        {QStringLiteral("nasa"), QStringLiteral("NASA"),
         QStringLiteral("science"), QUrl(QStringLiteral("https://www.nasa.gov/feed/")), true},
        {QStringLiteral("jpl"), QStringLiteral("NASA JPL"),
         QStringLiteral("science"), QUrl(QStringLiteral("https://www.jpl.nasa.gov/feeds/news/")), true}
    };
}

bool NewsCompanion::quietHours() const {
    const int hour = QTime::currentTime().hour();
    const int start = QSettings().value(QStringLiteral("news/quiet_start_hour"), 23).toInt();
    const int end = QSettings().value(QStringLiteral("news/quiet_end_hour"), 8).toInt();
    if(start == end) return false;
    if(start < end) return hour >= start && hour < end;
    return hour >= start || hour < end;
}

void NewsCompanion::scheduleNext(int delayMinutes) {
    if(!enabled()) {
        timer_.stop();
        return;
    }
    const int minutes = delayMinutes > 0 ? delayMinutes : intervalMinutes();
    timer_.start(qMax(1, minutes) * 60 * 1000);
}

void NewsCompanion::fetchNow(bool userInitiated) {
    if(fetching_) return;
    if(!userInitiated && !enabled()) return;

    if(!userInitiated) {
        if(quietHours()) {
            scheduleNext(30);
            return;
        }
        if(!pet_ || !pet_->autonomousSurfaceAvailable() ||
           pet_->autonomyInactivityMs() < 90 * 1000) {
            scheduleNext(15);
            return;
        }
    }

    userInitiated_ = userInitiated;
    fetchQueue_.clear();
    for(int i = 0; i < sources_.size(); ++i) {
        if(sourceEnabled(sources_.at(i).id))
            fetchQueue_.push_back(i);
    }

    if(fetchQueue_.isEmpty()) {
        emit statusChanged(QStringLiteral("No news sources are enabled."));
        if(userInitiated_ && pet_) {
            const bool zh = QSettings().value(
                QStringLiteral("ui/language"), QStringLiteral("en"))
                .toString().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive);
            pet_->showAutonomyNotice(
                zh ? QStringLiteral("还没有启用任何新闻源。")
                   : QStringLiteral("No news sources are enabled yet."));
        }
        scheduleNext();
        return;
    }

    // Rotate the first source so one publisher cannot dominate the companion.
    QSettings settings;
    const int cursor = settings.value(QStringLiteral("news/source_cursor"), 0).toInt();
    if(fetchQueue_.size() > 1) {
        const int shift = qAbs(cursor) % fetchQueue_.size();
        std::rotate(fetchQueue_.begin(), fetchQueue_.begin() + shift, fetchQueue_.end());
    }
    settings.setValue(QStringLiteral("news/source_cursor"), cursor + 1);

    fetchQueuePos_ = 0;
    fetching_ = true;
    emit statusChanged(QStringLiteral("Checking trusted news feeds..."));
    if(userInitiated_ && pet_) {
        const bool zh = QSettings().value(
            QStringLiteral("ui/language"), QStringLiteral("en"))
            .toString().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive);
        pet_->showAutonomyNotice(
            zh ? QStringLiteral("我去网上看看有没有新的消息。")
               : QStringLiteral("Let me check the news feeds for something new."));
    }
    fetchNextSource();
}

void NewsCompanion::fetchNextSource() {
    if(fetchQueuePos_ >= fetchQueue_.size()) {
        fetching_ = false;
        emit statusChanged(QStringLiteral("No unseen headlines right now."));
        if(userInitiated_ && pet_) {
            const bool zh = QSettings().value(
                QStringLiteral("ui/language"), QStringLiteral("en"))
                .toString().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive);
            pet_->showAutonomyNotice(
                zh ? QStringLiteral("暂时没有我还没说过的新标题，或者新闻源现在连接不上。")
                   : QStringLiteral("I couldn't find an unseen headline right now, or the feeds are temporarily unavailable."));
        }
        scheduleNext();
        return;
    }

    const Source source = sources_.at(fetchQueue_.at(fetchQueuePos_++));
    auto *reply = network_.get(requestFor(source.url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, source]{
        const QByteArray raw = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString errorText = reply->errorString();
        reply->deleteLater();

        if(ok) {
            const auto items = parseFeed(raw, source);
            for(const auto &item : items) {
                if(item.title.isEmpty() || isSeen(item)) continue;

                markSeen(item);
                latestUrl_ = item.url;
                latestTitle_ = item.title;
                latestSource_ = item.source;

                QSettings s;
                s.setValue(QStringLiteral("news/latest_url"), latestUrl_);
                s.setValue(QStringLiteral("news/latest_title"), latestTitle_);
                s.setValue(QStringLiteral("news/latest_source"), latestSource_);

                fetching_ = false;
                emit latestStoryChanged(latestTitle_, latestSource_, latestUrl_);
                emit headlineReady(latestTitle_, latestSource_, latestUrl_);
                emit statusChanged(QStringLiteral("Shared a new headline from %1.").arg(latestSource_));
                AppLogger::recordOperatorEvent(
                    QStringLiteral("news_headline_selected"),
                    latestTitle_,
                    QJsonObject{
                        {QStringLiteral("source"), latestSource_},
                        {QStringLiteral("url_host"), latestUrl_.host()},
                        {QStringLiteral("user_initiated"), userInitiated_}
                    });
                scheduleNext();
                return;
            }
        } else {
            qWarning().noquote() << "News feed failed" << source.name << errorText;
        }

        fetchNextSource();
    });
}

QVector<NewsCompanion::Item> NewsCompanion::parseFeed(
    const QByteArray &raw, const Source &source) const {
    QVector<Item> items;
    QXmlStreamReader xml(raw);

    bool insideItem = false;
    Item current;
    while(!xml.atEnd()) {
        xml.readNext();

        if(xml.isStartElement()) {
            const QString name = xml.name().toString().toLower();
            if(name == QStringLiteral("item") || name == QStringLiteral("entry")) {
                insideItem = true;
                current = Item{};
                current.source = source.name;
                continue;
            }
            if(!insideItem) continue;

            if(name == QStringLiteral("title")) {
                current.title = cleanText(xml.readElementText(QXmlStreamReader::SkipChildElements));
            } else if(name == QStringLiteral("guid") || name == QStringLiteral("id")) {
                current.stableId = cleanText(xml.readElementText(QXmlStreamReader::SkipChildElements));
            } else if(name == QStringLiteral("link")) {
                const QString href = xml.attributes().value(QStringLiteral("href")).toString().trimmed();
                if(!href.isEmpty()) current.url = QUrl(href);
                else {
                    const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                    if(!value.isEmpty()) current.url = QUrl(value);
                }
            }
        } else if(xml.isEndElement() && insideItem) {
            const QString name = xml.name().toString().toLower();
            if(name == QStringLiteral("item") || name == QStringLiteral("entry")) {
                if(current.url.scheme().toLower() != QStringLiteral("https"))
                    current.url = {};
                if(current.stableId.isEmpty())
                    current.stableId = current.url.toString();
                if(current.stableId.isEmpty())
                    current.stableId = current.title;
                if(!current.title.isEmpty()) items.push_back(current);
                insideItem = false;
                if(items.size() >= 12) break;
            }
        }
    }

    if(xml.hasError()) {
        qWarning().noquote() << "News RSS parse warning for" << source.name << xml.errorString();
    }
    return items;
}

QString NewsCompanion::itemKey(const Item &item) const {
    const QByteArray material =
        item.source.toUtf8() + '\n' + item.stableId.toUtf8() + '\n' + item.title.toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex());
}

bool NewsCompanion::isSeen(const Item &item) const {
    return QSettings().value(QStringLiteral("news/seen_keys")).toStringList()
        .contains(itemKey(item));
}

void NewsCompanion::markSeen(const Item &item) {
    QSettings s;
    QStringList keys = s.value(QStringLiteral("news/seen_keys")).toStringList();
    keys.removeAll(itemKey(item));
    keys.prepend(itemKey(item));
    while(keys.size() > 120) keys.removeLast();
    s.setValue(QStringLiteral("news/seen_keys"), keys);
}

void NewsCompanion::openLatestStory() {
    if(latestUrl_.isValid() && latestUrl_.scheme().toLower() == QStringLiteral("https"))
        QDesktopServices::openUrl(latestUrl_);
}
