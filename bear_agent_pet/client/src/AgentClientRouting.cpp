#define sendMessage sendMessageViaRouter
#include "AgentClient.h"
#undef sendMessage

#include "AppLogger.h"
#include "TonyResponseRouter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

void AgentClient::sendMessageViaRouter(const QString &text) {
    const QString prompt=text.trimmed();
    if(prompt.isEmpty()) return;

    TonyBehaviorEngine::Snapshot state;
    state.energy=78;
    state.warmth=68;
    state.affection=62;
    state.loneliness=18;
    state.curiosity=58;
    state.mood=QStringLiteral("content");

    TonyResponseRouter router;
    const auto decision=router.resolve(prompt,language_,connected(),state);

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
