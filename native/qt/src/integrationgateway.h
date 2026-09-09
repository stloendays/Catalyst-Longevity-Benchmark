#pragma once

#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class QTcpServer;
class QTcpSocket;

namespace catalyst {

struct IntegrationGatewayConfig {
    bool enabled = true;
    QString bindAddress = QStringLiteral("127.0.0.1");
    quint16 controlPort = 49321;
    quint16 eventPort = 49322;
    quint16 instrumentPort = 49323;
    QString accessToken;
};

class IntegrationGateway final : public QObject {
    Q_OBJECT

public:
    explicit IntegrationGateway(QObject* parent = nullptr);

    static IntegrationGatewayConfig configFromArguments(const QStringList& arguments);

    bool start(const IntegrationGatewayConfig& config, QString* errorMessage = nullptr);
    void stop();

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] QString accessToken() const;
    [[nodiscard]] QString bindAddressText() const;
    [[nodiscard]] quint16 controlPort() const;
    [[nodiscard]] quint16 eventPort() const;
    [[nodiscard]] quint16 instrumentPort() const;
    [[nodiscard]] QJsonObject capabilities() const;

    void publishEvent(const QString& type, const QJsonObject& payload = QJsonObject{});

signals:
    void statusChanged(const QString& text);
    void jobEnvelopeReceived(const QJsonObject& envelope);
    void resultEnvelopeReceived(const QJsonObject& envelope);
    void instrumentMessageReceived(const QJsonObject& message);

private:
    void acceptControlConnections();
    void acceptEventConnections();
    void acceptInstrumentConnections();
    void readControlSocket(QTcpSocket* socket);
    void readJsonLineSocket(QTcpSocket* socket, bool instrumentChannel);
    void removeSocket(QTcpSocket* socket);

    bool tokenMatches(const QString& supplied) const;
    bool authorizeJsonLine(QTcpSocket* socket, const QJsonObject& object, const QString& channel);
    void sendJsonLine(QTcpSocket* socket, const QJsonObject& object);
    void sendHttpJson(QTcpSocket* socket, int statusCode, const QJsonObject& object);
    QJsonObject gatewayState() const;

    QTcpServer* controlServer_ = nullptr;
    QTcpServer* eventServer_ = nullptr;
    QTcpServer* instrumentServer_ = nullptr;
    QHash<QTcpSocket*, QByteArray> controlBuffers_;
    QHash<QTcpSocket*, QByteArray> jsonLineBuffers_;
    QSet<QTcpSocket*> eventClients_;

    IntegrationGatewayConfig config_;
    QHostAddress bindAddress_;
    QString accessToken_;
};

} // namespace catalyst
