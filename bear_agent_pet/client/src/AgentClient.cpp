#include "AgentClient.h"
#include <QJsonDocument>
#include <QJsonObject>
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

void AgentClient::connectTo(const QUrl &url) {
    endpoint_=url;
    outageReported_=false;
    reconnect();
}

void AgentClient::reconnect() {
    if(!endpoint_.isValid() || socket_.state()!=QAbstractSocket::UnconnectedState) return;
    socket_.open(endpoint_);
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
