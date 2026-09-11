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
    void requestDevicePairing(const QUrl &wsUrl, const QString &deviceName);
    void pairAndConnect(const QUrl &wsUrl, const QString &pairingCode, const QString &deviceName);
    void sendMessage(const QString &text);
    bool sendOperatorLogBatch(const QString &batchId,
                              const QString &targetVersion,
                              const QByteArray &jsonl);
    void sendToolResult(const QString &requestId,
                        const QString &tool,
                        bool ok,
                        const QJsonObject &result={},
                        const QString &error={});
    void setLanguage(const QString &language);
    QString language() const { return language_; }
    bool connected() const;
    QString bearerToken() const { return bearerToken_; }
signals:
    void stateChanged(const QString &state);
    void avatarAction(const QString &action, const QString &emotion, int durationMs);
    void textDelta(const QString &text);
    void answerFinished();
    void connectionChanged(bool connected);
    void connectionStageChanged(const QString &stage);
    void pairingCodeReady(const QString &code, qint64 expiresAtEpochSeconds);
    void paired(const QString &token, const QString &deviceId, const QUrl &endpoint);
    void pairingFailed(const QString &text);
    void toolRequest(const QString &requestId, const QString &tool, const QJsonObject &args);
    void errorMessage(const QString &text);
private slots:
    void onText(const QString &message);
    void reconnect();
    void pollDevicePairing();
private:
    static QUrl pairingUrlFor(const QUrl &wsUrl);
    static QUrl pairingRequestUrlFor(const QUrl &wsUrl);
    static QUrl pairingStatusUrlFor(const QUrl &wsUrl);
    void sendClientHello();
    void scheduleReconnect();

    QWebSocket socket_;
    QNetworkAccessManager network_;
    QUrl endpoint_;
    QTimer reconnectTimer_;
    QTimer connectWatchdog_;
    QTimer pairingPollTimer_;
    QUrl pairingWsUrl_;
    QString pairingRequestId_;
    qint64 pairingExpiresAt_{0};
    bool pairingPollInFlight_{false};
    QString bearerToken_;
    QString language_{"en"};
    bool outageReported_{false};
    int reconnectDelayMs_{1000};
};