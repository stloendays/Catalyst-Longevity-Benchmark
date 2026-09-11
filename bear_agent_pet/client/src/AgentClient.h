#pragma once
#include <QObject>
#include <QWebSocket>
#include <QUrl>

class AgentClient : public QObject {
    Q_OBJECT
public:
    explicit AgentClient(QObject *parent=nullptr);
    void connectTo(const QUrl &url);
    void sendMessage(const QString &text);
    bool connected() const;
signals:
    void stateChanged(const QString &state);
    void textDelta(const QString &text);
    void answerFinished();
    void connectionChanged(bool connected);
    void errorMessage(const QString &text);
private slots:
    void onText(const QString &message);
private:
    QWebSocket socket_;
};
