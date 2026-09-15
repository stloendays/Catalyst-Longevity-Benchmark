#define sendMessage sendMessageViaRouter
#include "AgentClient.h"
#undef sendMessage

#include "AppLogger.h"
#include "TonyResponseRouter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>
#include <QtMath>

namespace {
TonyBehaviorEngine::Snapshot savedTonyState() {
    QSettings settings;
    auto read = [&](const char *key, double fallback) {
        return qBound(0, qRound(settings.value(QStringLiteral("tony/life/") + QString::fromLatin1(key), fallback).toDouble()), 100);
    };

    TonyBehaviorEngine::Snapshot state;
    state.energy=read("energy",78.0);
    state.warmth=read("warmth",68.0);
    state.affection=read("affection",62.0);
    state.loneliness=read("loneliness",18.0);
    state.curiosity=read("curiosity",58.0);

    if(state.warmth<35) state.mood=QStringLiteral("cold");
    else if(state.energy<32) state.mood=QStringLiteral("sleepy");
    else if(state.loneliness>56) state.mood=QStringLiteral("cuddly");
    else if(state.curiosity>74) state.mood=QStringLiteral("curious");
    else if(state.affection>82) state.mood=QStringLiteral("happy");
    else state.mood=QStringLiteral("content");
    return state;
}
}

void AgentClient::sendMessageViaRouter(const QString &text) {
    const QString prompt=text.trimmed();
    if(prompt.isEmpty()) return;

    TonyResponseRouter router;
    const auto decision=router.resolve(prompt,language_,connected(),savedTonyState());

    AppLogger::recordOperatorEvent(
        QStringLiteral("chat_route"),
        prompt,
        QJsonObject{
            {QStringLiteral("route"),TonyResponseRouter::routeName(decision.route)},
            {QStringLiteral("intent"),decision.intent},
            {QStringLiteral("connected"),connected()}
        });

    if(decision.handledLocally()) {
        emit stateChanged(QStringLiteral("thinking"));
        if(!decision.action.isEmpty())
            emit avatarAction(decision.action,decision.emotion,decision.durationMs);
        if(!decision.reply.isEmpty())
            emit textDelta(decision.reply);
        emit answerFinished();
        emit stateChanged(QStringLiteral("idle"));
        return;
    }

    if(!connected()) {
        if(!outageReported_) {
            outageReported_=true;
            emit errorMessage(language_==QStringLiteral("zh")
                ? QStringLiteral("这个问题需要服务器 Agent，但 Tony 现在还没有连接。")
                : QStringLiteral("That needs the server Agent, but Tony is not connected yet."));
        }
        return;
    }

    const QJsonObject payload{
        {QStringLiteral("type"),QStringLiteral("message")},
        {QStringLiteral("id"),QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("content"),decision.forwardText.isEmpty() ? prompt : decision.forwardText},
        {QStringLiteral("language"),language_},
        {QStringLiteral("client_route"),QStringLiteral("server_agent")}
    };
    socket_.sendTextMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}
