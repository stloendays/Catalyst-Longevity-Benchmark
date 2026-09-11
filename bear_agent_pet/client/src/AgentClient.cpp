#include "AgentClient.h"

#include "AppLogger.h"

#include <QCoreApplication>
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
}

AgentClient::AgentClient(QObject *parent): QObject(parent) {
    reconnectTimer_.setInterval(3000);
    reconnectTimer_.setSingleShot(false);
    connect(&reconnectTimer_, &QTimer::timeout, this, &AgentClient::reconnect);

    connect(&socket_, &QWebSocket::connected, this, [this]{
        reconnectTimer_.stop();
        outageReported_=false;
        sendClientHello();
        emit connectionChanged(true);
    });
    connect(&socket_, &QWebSocket::disconnected, this, [this]{
        emit connectionChanged(false);
        if(endpoint_.isValid() && !reconnectTimer_.isActive()) reconnectTimer_.start();
    });
    connect(&socket_, &QWebSocket::textMessageReceived, this, &AgentClient::onText);
    connect(&socket_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError){
        if(!outageReported_){
            outageReported_=true;
            emit errorMessage(socket_.errorString());
        }
        if(endpoint_.isValid() && socket_.state()==QAbstractSocket::UnconnectedState && !reconnectTimer_.isActive())
            reconnectTimer_.start();
    });
}

void AgentClient::setLanguage(const QString &language) {
    const QString n=language.trimmed().toLower();
    language_=(n.startsWith("zh") || n=="cn") ? "zh" : "en";
    if(connected()) sendClientHello();
}

void AgentClient::connectTo(const QUrl &url, const QString &bearerToken) {
    const bool changed = endpoint_ != url || bearerToken_ != bearerToken;
    endpoint_=url;
    bearerToken_=bearerToken.trimmed();
    outageReported_=false;
    reconnectTimer_.stop();

    if(changed && socket_.state()!=QAbstractSocket::UnconnectedState) {
        socket_.close();
        return;
    }
    reconnect();
}

QUrl AgentClient::pairingUrlFor(const QUrl &wsUrl) {
    QUrl url(wsUrl);
    if(url.scheme().compare("wss", Qt::CaseInsensitive)==0) url.setScheme("https");
    else if(url.scheme().compare("ws", Qt::CaseInsensitive)==0) url.setScheme("http");
    else return {};
    url.setPath("/pair");
    url.setQuery({});
    url.setFragment({});
    return url;
}

void AgentClient::pairAndConnect(const QUrl &wsUrl, const QString &pairingCode, const QString &deviceName) {
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
    const QJsonObject body{
        {"code",pairingCode.trimmed()},
        {"device_name",deviceName.trimmed().isEmpty() ? QString("Tony desktop") : deviceName.trimmed()}
    };
    auto *reply=network_.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[this,reply,wsUrl]{
        const QByteArray raw=reply->readAll();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto doc=QJsonDocument::fromJson(raw);
        const auto obj=doc.isObject() ? doc.object() : QJsonObject{};

        if(reply->error()!=QNetworkReply::NoError || status<200 || status>=300) {
            QString detail=obj.value("detail").toString();
            if(detail.isEmpty()) detail=reply->errorString();
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

void AgentClient::reconnect() {
    if(!endpoint_.isValid() || socket_.state()!=QAbstractSocket::UnconnectedState) return;
    QNetworkRequest request(endpoint_);
    request.setHeader(QNetworkRequest::UserAgentHeader, tonyUserAgent());
    if(!bearerToken_.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + bearerToken_.toUtf8());
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
        {"capabilities",QJsonArray{QStringLiteral("operator_log_sync_v1")}}
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
