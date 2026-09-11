#pragma once
#include <QObject>
#include <QWebSocket>
#include <QUrl>
#include <QTimer>

class AgentClient : public QObject {
    Q_OBJECT
public:
    explicit AgentClient(QObject *parent=nullptr);
    void connectTo(const QUrl &url);
    void sendMessage(const QString &text);
    bool connected() const;
signals:
    void stateChanged(const QString &state);
    void avatarAction(const QString &action, const QString &emotion, int durationMs);
    void textDelta(const QString &text);
    void answerFinished();
    void connectionChanged(bool connected);
    void errorMessage(const QString &text);
private slots:
    void onText(const QString &message);
    void reconnect();
private:
    QWebSocket socket_;
    QUrl endpoint_;
    QTimer reconnectTimer_;
};
