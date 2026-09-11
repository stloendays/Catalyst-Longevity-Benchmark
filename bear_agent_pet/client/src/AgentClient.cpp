#include "AgentClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

AgentClient::AgentClient(QObject *parent): QObject(parent) {
    connect(&socket_, &QWebSocket::connected, this, [this]{ emit connectionChanged(true); });
    connect(&socket_, &QWebSocket::disconnected, this, [this]{ emit connectionChanged(false); });
    connect(&socket_, &QWebSocket::textMessageReceived, this, &AgentClient::onText);
    connect(&socket_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError){ emit errorMessage(socket_.errorString()); });
}

void AgentClient::connectTo(const QUrl &url) { socket_.open(url); }
bool AgentClient::connected() const { return socket_.state() == QAbstractSocket::ConnectedState; }
void AgentClient::sendMessage(const QString &text) {
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
