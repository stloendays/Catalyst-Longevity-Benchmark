#include "AgentClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

AgentClient::AgentClient(QObject *parent): QObject(parent) {
    reconnectTimer_.setInterval(3000);
    reconnectTimer_.setSingleShot(false);
    connect(&reconnectTimer_, &QTimer::timeout, this, &AgentClient::reconnect);

    connect(&socket_, &QWebSocket::connected, this, [this]{
        reconnectTimer_.stop();
        outageReported_=false;
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
        emit pairingFailed("连接地址不正确，请使用 ws:// 或 wss:// 地址。");
        return;
    }

    QNetworkRequest request(pairUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "TonyDesktopPet/0.4");
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
            emit pairingFailed(QString("配对失败：%1").arg(detail));
            reply->deleteLater();
            return;
        }

        const QString token=obj.value("token").toString().trimmed();
        const QString deviceId=obj.value("device_id").toString().trimmed();
        if(token.isEmpty()) {
            emit pairingFailed("配对响应里没有设备令牌。");
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
    request.setHeader(QNetworkRequest::UserAgentHeader, "TonyDesktopPet/0.4");
    if(!bearerToken_.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + bearerToken_.toUtf8());
    socket_.open(request);
}

bool AgentClient::connected() const { return socket_.state() == QAbstractSocket::ConnectedState; }

void AgentClient::sendMessage(const QString &text) {
    if(!connected()) {
        if(!outageReported_){
            outageReported_=true;
            emit errorMessage("Tony Agent is not connected yet.");
        }
        return;
    }
    QJsonObject o{{"type","message"},{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"content",text}};
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
    } else if(type=="final" || type=="answer_done") {
        emit answerFinished();
    } else if(type=="error") {
        emit errorMessage(o.value("message").toString());
    }
}
