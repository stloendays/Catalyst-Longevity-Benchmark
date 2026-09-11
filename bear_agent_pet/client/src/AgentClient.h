#pragma once
#include <QObject>
#include <QWebSocket>
#include <QUrl>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QJsonObject>

class AgentClient : public QObject {
    Q_OBJECT
public:
    explicit AgentClient(QObject *parent=nullptr);
    void connectTo(const QUrl &url, const QString &bearerToken={});
    void pairAndConnect(const QUrl &wsUrl, const QString &pairingCode, const QString &deviceName);
    void sendMessage(const QString &text);
    void sendToolResult(const QString &requestId,
                        const QString &tool,
                        bool ok,
                        const QJsonObject &result={},
                        const QString &error={});
    bool connected() const;
    QString bearerToken() const { return bearerToken_; }
signals:
    void stateChanged(const QString &state);
    void avatarAction(const QString &action, const QString &emotion, int durationMs);
    void textDelta(const QString &text);
    void answerFinished();
    void connectionChanged(bool connected);
    void paired(const QString &token, const QString &deviceId, const QUrl &endpoint);
    void pairingFailed(const QString &text);
    void toolRequest(const QString &requestId, const QString &tool, const QJsonObject &args);
    void errorMessage(const QString &text);
private slots:
    void onText(const QString &message);
    void reconnect();
private:
    static QUrl pairingUrlFor(const QUrl &wsUrl);
    void sendClientHello();

    QWebSocket socket_;
    QNetworkAccessManager network_;
    QUrl endpoint_;
    QTimer reconnectTimer_;
    QString bearerToken_;
    bool outageReported_{false};
};
