#include "AgentClient.h"

#include "AppLogger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QUuid>

namespace {
QByteArray tonyUserAgent() {
    return QByteArray("TonyDesktopPet/") + QCoreApplication::applicationVersion().toUtf8();
}

QUrl pairingApiUrlFor(const QUrl &wsUrl, const QString &path) {
    QUrl url(wsUrl);
    if(url.scheme().compare("wss", Qt::CaseInsensitive)==0) url.setScheme("https");
    else if(url.scheme().compare("ws", Qt::CaseInsensitive)==0) url.setScheme("http");
    else return {};
    url.setPath(path);
    url.setQuery({});
    url.setFragment({});
    return url;
}

QString friendlyTransportMessage(const QString &raw, const QUrl &url, bool zh) {
    const QString lower=raw.toLower();
    const bool secure=url.scheme().compare(QStringLiteral("wss"),Qt::CaseInsensitive)==0 ||
                      url.scheme().compare(QStringLiteral("https"),Qt::CaseInsensitive)==0;
    if(lower.contains(QStringLiteral("timed out")) || lower.contains(QStringLiteral("timeout"))) {
        if(secure) return zh ? QStringLiteral("无法连接 Tony 公网入口（超时）。请检查云服务器安全组是否放行 TCP 443。")
                             : QStringLiteral("Tony's public endpoint timed out. Check that the cloud security group allows inbound TCP 443.");
        return zh ? QStringLiteral("连接 Tony 超时。") : QStringLiteral("The Tony connection timed out.");
    }
    if(lower.contains(QStringLiteral("host not found")) || lower.contains(QStringLiteral("name or service")))
        return zh ? QStringLiteral("无法解析 Tony 服务器域名，请检查网络或服务器地址。")
                  : QStringLiteral("Tony's server name could not be resolved. Check the network or server address.");
    if(lower.contains(QStringLiteral("ssl")) || lower.contains(QStringLiteral("tls")) || lower.contains(QStringLiteral("handshake")))
        return zh ? QStringLiteral("Tony 的 TLS 安全连接失败。请检查 HTTPS 证书以及 TCP 443 是否可达。")
                  : QStringLiteral("Tony's TLS handshake failed. Check the HTTPS certificate and TCP 443 reachability.");
    if(lower.contains(QStringLiteral("refused")))
        return zh ? QStringLiteral("Tony 服务器拒绝连接，请检查反向代理和网关服务。")
                  : QStringLiteral("Tony's server refused the connection. Check the reverse proxy and gateway service.");
    return raw.trimmed().isEmpty()
        ? (zh ? QStringLiteral("Tony 网络连接失败。") : QStringLiteral("Tony's network connection failed."))
        : raw.trimmed();
}
}

AgentClient::AgentClient(QObject *parent): QObject(parent) {
    reconnectTimer_.setSingleShot(true);
    connect(&reconnectTimer_, &QTimer::timeout, this, &AgentClient::reconnect);

    connectWatchdog_.setSingleShot(true);
    connect(&connectWatchdog_, &QTimer::timeout, this, [this]{
        if(socket_.state()!=QAbstractSocket::ConnectingState) return;
        const QString detail=friendlyTransportMessage(
            language_=="zh" ? QStringLiteral("连接超时") : QStringLiteral("connection timed out"),
            endpoint_,language_=="zh");
        socket_.abort();
        if(!outageReported_) {
            outageReported_=true;
            emit errorMessage(detail);
        }
        emit connectionStageChanged(QStringLiteral("retrying"));
        scheduleReconnect();
    });

    // Re-arm only after each HTTPS status request completes. This stays reliable
    // when owner approval happens seconds, minutes, or hours after the code is shown.
    pairingPollTimer_.setInterval(2000);
    pairingPollTimer_.setSingleShot(true);
    connect(&pairingPollTimer_, &QTimer::timeout, this, &AgentClient::pollDevicePairing);

    connect(&socket_, &QWebSocket::connected, this, [this]{
        connectWatchdog_.stop();
        reconnectTimer_.stop();
        reconnectDelayMs_=1000;
        outageReported_=false;
        sendClientHello();
        emit connectionStageChanged(QStringLiteral("connected"));
        emit connectionChanged(true);
    });
    connect(&socket_, &QWebSocket::disconnected, this, [this]{
        connectWatchdog_.stop();
        emit connectionChanged(false);
        if(endpoint_.isValid() && !bearerToken_.isEmpty()) {
            emit connectionStageChanged(QStringLiteral("retrying"));
            scheduleReconnect();
        }
    });
    connect(&socket_, &QWebSocket::textMessageReceived, this, &AgentClient::onText);
    connect(&socket_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError){
        connectWatchdog_.stop();
        if(!outageReported_){
            outageReported_=true;
            emit errorMessage(friendlyTransportMessage(socket_.errorString(),endpoint_,language_=="zh"));
        }
        emit connectionStageChanged(QStringLiteral("retrying"));
        scheduleReconnect();
    });
}

void AgentClient::setLanguage(const QString &language) {
    const QString n=language.trimmed().toLower();
    language_=(n.startsWith("zh") || n=="cn") ? "zh" : "en";
    if(connected()) sendClientHello();
}

void AgentClient::connectTo(const QUrl &url, const QString &bearerToken) {
    pairingPollTimer_.stop();
    pairingRequestId_.clear();
    pairingExpiresAt_=0;
    pairingPollInFlight_=false;

    const bool changed = endpoint_ != url || bearerToken_ != bearerToken;
    endpoint_=url;
    bearerToken_=bearerToken.trimmed();
    outageReported_=false;
    reconnectDelayMs_=1000;
    connectWatchdog_.stop();
    reconnectTimer_.stop();

    if(changed && socket_.state()!=QAbstractSocket::UnconnectedState) {
        socket_.close();
        return;
    }
    reconnect();
}

QUrl AgentClient::pairingUrlFor(const QUrl &wsUrl) {
    return pairingApiUrlFor(wsUrl,QStringLiteral("/pair"));
}

QUrl AgentClient::pairingRequestUrlFor(const QUrl &wsUrl) {
    return pairingApiUrlFor(wsUrl,QStringLiteral("/pair/request"));
}

QUrl AgentClient::pairingStatusUrlFor(const QUrl &wsUrl) {
    return pairingApiUrlFor(wsUrl,QStringLiteral("/pair/status"));
}

void AgentClient::requestDevicePairing(const QUrl &wsUrl, const QString &deviceName) {
    const QUrl requestUrl=pairingRequestUrlFor(wsUrl);
    if(!requestUrl.isValid() || requestUrl.host().isEmpty()) {
        emit pairingFailed(language_=="zh" ? "连接地址不正确。" : "The Tony server address is invalid.");
        return;
    }

    pairingPollTimer_.stop();
    pairingRequestId_.clear();
    pairingWsUrl_=wsUrl;
    pairingExpiresAt_=0;
    pairingPollInFlight_=false;

    AppLogger::recordOperatorEvent(
        QStringLiteral("device_pair_request"),
        {},
        QJsonObject{{QStringLiteral("transport"), requestUrl.scheme().toLower()}});

    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader,tonyUserAgent());
    request.setTransferTimeout(12000);
    emit connectionStageChanged(QStringLiteral("pairing_request"));
    const QJsonObject body{
        {"device_name",deviceName.trimmed().isEmpty() ? QString("Tony desktop") : deviceName.trimmed()}
    };
    auto *reply=network_.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[this,reply,requestUrl]{
        const QByteArray raw=reply->readAll();
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto doc=QJsonDocument::fromJson(raw);
        const auto obj=doc.isObject() ? doc.object() : QJsonObject{};
        if(reply->error()!=QNetworkReply::NoError || status<200 || status>=300) {
            QString detail=obj.value("detail").toString();
            if(detail.isEmpty()) detail=friendlyTransportMessage(reply->errorString(),requestUrl,language_=="zh");
            emit connectionStageChanged(QStringLiteral("offline"));
            emit pairingFailed((language_=="zh" ? QString("无法生成连接码：") : QString("Could not create a connection code: "))+detail);
            reply->deleteLater();
            return;
        }

        pairingRequestId_=obj.value("request_id").toString().trimmed();
        const QString code=obj.value("code").toString().trimmed();
        pairingExpiresAt_=static_cast<qint64>(obj.value("expires_at").toDouble(0));
        const int pollMs=qBound(1000,obj.value("poll_after_ms").toInt(2000),5000);
        if(pairingRequestId_.isEmpty() || code.isEmpty() || pairingExpiresAt_<=QDateTime::currentSecsSinceEpoch()) {
            pairingRequestId_.clear();
            emit pairingFailed(language_=="zh" ? "服务器没有返回有效的连接码。" : "The server did not return a valid connection code.");
            reply->deleteLater();
            return;
        }

        pairingPollTimer_.setInterval(pollMs);
        emit connectionStageChanged(QStringLiteral("approval_required"));
        emit pairingCodeReady(code,pairingExpiresAt_);
        reply->deleteLater();

        // Check once almost immediately, then each completed pending response
        // re-arms the single-shot timer below.
        QTimer::singleShot(250, this, [this]{
            if(!pairingRequestId_.isEmpty() && !pairingPollInFlight_)
                pollDevicePairing();
        });
    });
}

void AgentClient::pollDevicePairing() {
    if(pairingRequestId_.isEmpty() || !pairingWsUrl_.isValid()) return;
    if(pairingPollInFlight_) {
        pairingPollTimer_.start(500);
        return;
    }
    if(pairingExpiresAt_>0 && QDateTime::currentSecsSinceEpoch()>=pairingExpiresAt_) {
        pairingPollTimer_.stop();
        pairingRequestId_.clear();
        emit pairingFailed(language_=="zh" ? "连接码已过期，请生成一个新的。" : "The connection code expired. Generate a new one.");
        return;
    }

    const QUrl statusUrl=pairingStatusUrlFor(pairingWsUrl_);
    if(!statusUrl.isValid()) return;
    pairingPollInFlight_=true;
    QNetworkRequest request(statusUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader,tonyUserAgent());
    request.setTransferTimeout(8000);
    const QJsonObject body{{"request_id",pairingRequestId_}};
    auto *reply=network_.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[this,reply]{
        pairingPollInFlight_=false;
        const QByteArray raw=reply->readAll();
        const int httpStatus=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto doc=QJsonDocument::fromJson(raw);
        const auto obj=doc.isObject() ? doc.object() : QJsonObject{};

        if(httpStatus==410) {
            pairingPollTimer_.stop();
            pairingRequestId_.clear();
            emit pairingFailed(language_=="zh" ? "连接码已过期，请生成一个新的。" : "The connection code expired. Generate a new one.");
            reply->deleteLater();
            return;
        }
        if(httpStatus==404) {
            pairingPollTimer_.stop();
            pairingRequestId_.clear();
            QString detail=obj.value("detail").toString();
            if(detail.isEmpty()) detail=language_=="zh" ? "连接请求不存在。" : "The pairing request was not found.";
            emit pairingFailed(detail);
            reply->deleteLater();
            return;
        }
        if(reply->error()!=QNetworkReply::NoError || httpStatus<200 || httpStatus>=300) {
            // Retry only after the previous HTTPS request has completed.
            reply->deleteLater();
            if(!pairingRequestId_.isEmpty()) pairingPollTimer_.start();
            return;
        }

        if(obj.value("status").toString()!=QStringLiteral("approved")) {
            reply->deleteLater();
            if(!pairingRequestId_.isEmpty()) pairingPollTimer_.start();
            return;
        }

        const QString token=obj.value("token").toString().trimmed();
        const QString deviceId=obj.value("device_id").toString().trimmed();
        if(token.isEmpty()) {
            pairingPollTimer_.stop();
            emit pairingFailed(language_=="zh" ? "批准响应中没有设备令牌。" : "The approval response did not contain a device token.");
            reply->deleteLater();
            return;
        }

        const QUrl endpoint=pairingWsUrl_;
        pairingPollTimer_.stop();
        pairingRequestId_.clear();
        AppLogger::recordOperatorEvent(QStringLiteral("device_pair_approved"));
        emit connectionStageChanged(QStringLiteral("connecting"));
        emit paired(token,deviceId,endpoint);
        connectTo(endpoint,token);
        reply->deleteLater();
    });
}

void AgentClient::pairAndConnect(const QUrl &wsUrl, const QString &pairingCode, const QString &deviceName) {
    pairingPollTimer_.stop();
    pairingRequestId_.clear();
    pairingExpiresAt_=0;
    pairingPollInFlight_=false;

    const QUrl pairUrl=pairingUrlFor(wsUrl);
    if(!pairUrl.isValid() || pairUrl.host().isEmpty()) {
        emit pairingFailed(language_=="zh" ? "连接地址不正确。" : "The Tony server address is invalid.");
        return;
    }

    AppLogger::recordOperatorEvent(
        QStringLiteral("pair_attempt"),
        {},
        QJsonObject{{QStringLiteral("transport"), pairUrl.scheme().toLower()}});

    QNetworkRequest request(pairUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, tonyUserAgent());
    request.setTransferTimeout(12000);
    const QJsonObject body{
        {"code",pairingCode.trimmed()},
        {"device_name",deviceName.trimmed().isEmpty() ? QString("Tony desktop") : deviceName.trimmed()}
    };
    auto *reply=network_.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[this,reply,wsUrl,pairUrl]{
        const QByteArray raw=reply->readAll();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto doc=QJsonDocument::fromJson(raw);
        const auto obj=doc.isObject() ? doc.object() : QJsonObject{};

        if(reply->error()!=QNetworkReply::NoError || status<200 || status>=300) {
            QString detail=obj.value("detail").toString();
            if(detail.isEmpty()) detail=friendlyTransportMessage(reply->errorString(),pairUrl,language_=="zh");
            emit pairingFailed((language_=="zh" ? QString("配对失败：") : QString("Pairing failed: "))+detail);
            reply->deleteLater();
            return;
        }

        const QString token=obj.value("token").toString().trimmed();
        const QString deviceId=obj.value("device_id").toString().trimmed();
        if(token.isEmpty()) {
            emit pairingFailed(language_=="zh" ? "配对响应中没有设备令牌。" : "The pairing response did not contain a device token.");
            reply->deleteLater();
            return;
        }

        emit paired(token,deviceId,wsUrl);
        connectTo(wsUrl,token);
        reply->deleteLater();
    });
}

void AgentClient::scheduleReconnect() {
    if(!endpoint_.isValid() || bearerToken_.isEmpty() || reconnectTimer_.isActive()) return;
    reconnectTimer_.start(reconnectDelayMs_);
    reconnectDelayMs_=qMin(reconnectDelayMs_*2,15000);
}

void AgentClient::reconnect() {
    if(!endpoint_.isValid() || bearerToken_.isEmpty() || socket_.state()!=QAbstractSocket::UnconnectedState) return;
    emit connectionStageChanged(QStringLiteral("connecting"));
    QNetworkRequest request(endpoint_);
    request.setHeader(QNetworkRequest::UserAgentHeader, tonyUserAgent());
    request.setRawHeader("Authorization", QByteArray("Bearer ") + bearerToken_.toUtf8());
    connectWatchdog_.start(12000);
    socket_.open(request);
}

bool AgentClient::connected() const { return socket_.state() == QAbstractSocket::ConnectedState; }

void AgentClient::sendClientHello() {
    if(!connected()) return;
    QJsonObject o{
        {"type","client_hello"},
        {"protocol_version","1"},
        {"client","TonyDesktopPet"},
        {"client_version",QCoreApplication::applicationVersion()},
        {"device_name",QSysInfo::machineHostName()},
        {"platform",QSysInfo::productType()},
        {"language",language_},
        {"capabilities",QJsonArray{QStringLiteral("operator_log_sync_v1"),QStringLiteral("device_code_pairing_v1")}}
    };
    socket_.sendTextMessage(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void AgentClient::sendMessage(const QString &text) {
    if(!connected()) {
        if(!outageReported_){
            outageReported_=true;
            emit errorMessage(language_=="zh" ? "Tony 还没有连接。" : "Tony is not connected yet.");
        }
        return;
    }
    QJsonObject o{
        {"type","message"},
        {"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"content",text},
        {"language",language_}
    };
    socket_.sendTextMessage(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool AgentClient::sendOperatorLogBatch(const QString &batchId,
                                       const QString &targetVersion,
                                       const QByteArray &jsonl) {
    if(!connected() || jsonl.isEmpty()) return false;

    QJsonObject o{
        {"type","operator_log_batch"},
        {"batch_id",batchId.trimmed().toLower()},
        {"source_version",QCoreApplication::applicationVersion()},
        {"target_version",targetVersion.trimmed().left(40)},
        {"content_format","jsonl-v1"},
        {"content",QString::fromUtf8(jsonl)}
    };
    const QString frame = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    const qint64 queued = socket_.sendTextMessage(frame);
    if(queued > 0)
        qInfo().noquote() << "Queued encrypted-archive operator log batch"
                          << batchId.left(12) << "bytes" << jsonl.size();
    return queued > 0;
}

void AgentClient::sendToolResult(const QString &requestId,
                                 const QString &tool,
                                 bool ok,
                                 const QJsonObject &result,
                                 const QString &error) {
    if(!connected()) return;
    QJsonObject o{
        {"type","tool_result"},
        {"request_id",requestId},
        {"tool",tool},
        {"ok",ok},
        {"result",result}
    };
    if(!error.isEmpty()) o.insert("error",error.left(1200));
    socket_.sendTextMessage(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void AgentClient::onText(const QString &message) {
    auto doc=QJsonDocument::fromJson(message.toUtf8());
    if(!doc.isObject()) return;
    auto o=doc.object();
    const auto type=o.value("type").toString();
    if(type=="agent_state") {
        emit stateChanged(o.value("state").toString());
    } else if(type=="avatar_action") {
        emit avatarAction(
            o.value("action").toString("idle"),
            o.value("emotion").toString("neutral"),
            o.value("duration_ms").toInt(0));
    } else if(type=="text_delta") {
        emit textDelta(o.value("content").toString());
    } else if(type=="tool_request") {
        const QString requestId=o.value("request_id").toString();
        const QString tool=o.value("tool").toString();
        const QJsonObject args=o.value("args").toObject();
        if(requestId.isEmpty() || tool.isEmpty()) {
            emit errorMessage(language_=="zh" ? "Tony 收到了不完整的本地工具请求。" : "Tony received an incomplete local tool request.");
        } else {
            emit toolRequest(requestId,tool,args);
        }
    } else if(type=="operator_log_ack") {
        const QString batchId=o.value("batch_id").toString().trimmed().toLower();
        if(AppLogger::markOperatorLogUploaded(batchId))
            qInfo().noquote() << "Operator log batch archived locally after server acknowledgement"
                              << batchId.left(12);
    } else if(type=="operator_log_rejected") {
        qWarning().noquote() << "Operator log batch rejected by server:"
                             << o.value("message").toString();
    } else if(type=="final" || type=="answer_done") {
        emit answerFinished();
    } else if(type=="error") {
        emit errorMessage(o.value("message").toString());
    }
}

