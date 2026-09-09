#include "integrationgateway.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

namespace catalyst {

namespace {

constexpr qsizetype kMaxHttpRequestBytes = 1024 * 1024;
constexpr qsizetype kMaxJsonLineBytes = 256 * 1024;

QString randomToken() {
    QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    token.remove(QChar('-'));
    QString second = QUuid::createUuid().toString(QUuid::WithoutBraces);
    second.remove(QChar('-'));
    return token + second;
}

quint16 parsedPort(const QString& text, quint16 fallback) {
    bool ok = false;
    const uint value = text.trimmed().toUInt(&ok);
    if (!ok || value == 0 || value > 65535) return fallback;
    return static_cast<quint16>(value);
}

QString argumentValue(const QStringList& arguments, const QString& prefix) {
    for (const auto& argument : arguments) {
        if (argument.startsWith(prefix)) return argument.mid(prefix.size());
    }
    return {};
}

QByteArray statusReason(int statusCode) {
    switch (statusCode) {
    case 200: return QByteArrayLiteral("OK");
    case 202: return QByteArrayLiteral("Accepted");
    case 400: return QByteArrayLiteral("Bad Request");
    case 401: return QByteArrayLiteral("Unauthorized");
    case 403: return QByteArrayLiteral("Forbidden");
    case 404: return QByteArrayLiteral("Not Found");
    case 405: return QByteArrayLiteral("Method Not Allowed");
    case 413: return QByteArrayLiteral("Payload Too Large");
    default: return QByteArrayLiteral("Error");
    }
}

QString compactTimestamp() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

} // namespace

IntegrationGateway::IntegrationGateway(QObject* parent)
    : QObject(parent),
      controlServer_(new QTcpServer(this)),
      eventServer_(new QTcpServer(this)),
      instrumentServer_(new QTcpServer(this)) {
    connect(controlServer_, &QTcpServer::newConnection, this, &IntegrationGateway::acceptControlConnections);
    connect(eventServer_, &QTcpServer::newConnection, this, &IntegrationGateway::acceptEventConnections);
    connect(instrumentServer_, &QTcpServer::newConnection, this, &IntegrationGateway::acceptInstrumentConnections);
}

IntegrationGatewayConfig IntegrationGateway::configFromArguments(const QStringList& arguments) {
    IntegrationGatewayConfig config;

    const QString envBind = qEnvironmentVariable("CATALYST_SDL_BIND");
    if (!envBind.trimmed().isEmpty()) config.bindAddress = envBind.trimmed();

    const QString envControl = qEnvironmentVariable("CATALYST_SDL_CONTROL_PORT");
    const QString envEvents = qEnvironmentVariable("CATALYST_SDL_EVENT_PORT");
    const QString envInstrument = qEnvironmentVariable("CATALYST_SDL_INSTRUMENT_PORT");
    const QString envToken = qEnvironmentVariable("CATALYST_SDL_TOKEN");
    if (!envControl.isEmpty()) config.controlPort = parsedPort(envControl, config.controlPort);
    if (!envEvents.isEmpty()) config.eventPort = parsedPort(envEvents, config.eventPort);
    if (!envInstrument.isEmpty()) config.instrumentPort = parsedPort(envInstrument, config.instrumentPort);
    if (!envToken.isEmpty()) config.accessToken = envToken;

    if (arguments.contains(QStringLiteral("--no-sdl-gateway"))) config.enabled = false;

    const QString bindArg = argumentValue(arguments, QStringLiteral("--sdl-bind="));
    const QString controlArg = argumentValue(arguments, QStringLiteral("--sdl-control-port="));
    const QString eventArg = argumentValue(arguments, QStringLiteral("--sdl-event-port="));
    const QString instrumentArg = argumentValue(arguments, QStringLiteral("--sdl-instrument-port="));
    const QString tokenArg = argumentValue(arguments, QStringLiteral("--sdl-token="));
    if (!bindArg.trimmed().isEmpty()) config.bindAddress = bindArg.trimmed();
    if (!controlArg.isEmpty()) config.controlPort = parsedPort(controlArg, config.controlPort);
    if (!eventArg.isEmpty()) config.eventPort = parsedPort(eventArg, config.eventPort);
    if (!instrumentArg.isEmpty()) config.instrumentPort = parsedPort(instrumentArg, config.instrumentPort);
    if (!tokenArg.isEmpty()) config.accessToken = tokenArg;

    return config;
}

bool IntegrationGateway::start(const IntegrationGatewayConfig& config, QString* errorMessage) {
    stop();
    config_ = config;

    if (!config_.enabled) {
        if (errorMessage) *errorMessage = QStringLiteral("自驱动实验室接口已禁用。");
        return true;
    }

    QHostAddress address;
    if (!address.setAddress(config_.bindAddress)) {
        if (errorMessage) *errorMessage = QStringLiteral("无效的 SDL 绑定地址：%1").arg(config_.bindAddress);
        return false;
    }
    bindAddress_ = address;

    if (!bindAddress_.isLoopback() && config_.accessToken.trimmed().size() < 24) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "绑定到非本机地址时必须显式提供至少 24 字符的 CATALYST_SDL_TOKEN 或 --sdl-token。"
            );
        }
        return false;
    }

    accessToken_ = config_.accessToken.trimmed().isEmpty() ? randomToken() : config_.accessToken.trimmed();

    const auto listen = [this, errorMessage](QTcpServer* server, quint16 port, const QString& name) {
        if (server->listen(bindAddress_, port)) return true;
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1端口启动失败：%2").arg(name, server->errorString());
        }
        return false;
    };

    if (!listen(controlServer_, config_.controlPort, QStringLiteral("控制 API"))) {
        stop();
        return false;
    }
    if (!listen(eventServer_, config_.eventPort, QStringLiteral("事件流"))) {
        stop();
        return false;
    }
    if (!listen(instrumentServer_, config_.instrumentPort, QStringLiteral("仪器桥"))) {
        stop();
        return false;
    }

    config_.controlPort = controlServer_->serverPort();
    config_.eventPort = eventServer_->serverPort();
    config_.instrumentPort = instrumentServer_->serverPort();

    const QString message = QStringLiteral("SDL 接口已启动：%1 · HTTP %2 · Events %3 · Instrument %4")
        .arg(config_.bindAddress)
        .arg(config_.controlPort)
        .arg(config_.eventPort)
        .arg(config_.instrumentPort);
    emit statusChanged(message);
    publishEvent(QStringLiteral("gateway.ready"), gatewayState());
    if (errorMessage) *errorMessage = message;
    return true;
}

void IntegrationGateway::stop() {
    const auto closeClients = [](const auto& sockets) {
        for (auto* socket : sockets) {
            if (!socket) continue;
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    };
    closeClients(controlBuffers_.keys());
    closeClients(jsonLineBuffers_.keys());
    controlBuffers_.clear();
    jsonLineBuffers_.clear();
    eventClients_.clear();

    if (controlServer_->isListening()) controlServer_->close();
    if (eventServer_->isListening()) eventServer_->close();
    if (instrumentServer_->isListening()) instrumentServer_->close();
    bindAddress_.clear();
}

bool IntegrationGateway::isRunning() const {
    return controlServer_->isListening() && eventServer_->isListening() && instrumentServer_->isListening();
}

QString IntegrationGateway::accessToken() const {
    return accessToken_;
}

QString IntegrationGateway::bindAddressText() const {
    return bindAddress_.isNull() ? config_.bindAddress : bindAddress_.toString();
}

quint16 IntegrationGateway::controlPort() const {
    return controlServer_->isListening() ? controlServer_->serverPort() : config_.controlPort;
}

quint16 IntegrationGateway::eventPort() const {
    return eventServer_->isListening() ? eventServer_->serverPort() : config_.eventPort;
}

quint16 IntegrationGateway::instrumentPort() const {
    return instrumentServer_->isListening() ? instrumentServer_->serverPort() : config_.instrumentPort;
}

QJsonObject IntegrationGateway::capabilities() const {
    QJsonObject ports;
    ports.insert(QStringLiteral("control_http"), static_cast<int>(controlPort()));
    ports.insert(QStringLiteral("events_jsonl"), static_cast<int>(eventPort()));
    ports.insert(QStringLiteral("instrument_jsonl"), static_cast<int>(instrumentPort()));

    QJsonObject safety;
    safety.insert(QStringLiteral("hardware_actuation"), false);
    safety.insert(QStringLiteral("job_execution"), QStringLiteral("not_enabled"));
    safety.insert(QStringLiteral("instrument_channel"), QStringLiteral("telemetry_and_adapter_ingress_only"));
    safety.insert(QStringLiteral("external_bind_requires_explicit_token"), true);

    QJsonArray httpEndpoints;
    httpEndpoints.append(QStringLiteral("GET /v1/health"));
    httpEndpoints.append(QStringLiteral("GET /v1/capabilities"));
    httpEndpoints.append(QStringLiteral("GET /v1/pairing (loopback only)"));
    httpEndpoints.append(QStringLiteral("GET /v1/state"));
    httpEndpoints.append(QStringLiteral("POST /v1/jobs"));
    httpEndpoints.append(QStringLiteral("POST /v1/results"));
    httpEndpoints.append(QStringLiteral("POST /v1/events"));

    QJsonObject result;
    result.insert(QStringLiteral("service"), QStringLiteral("catalyst-longevity-sdl-gateway"));
    result.insert(QStringLiteral("protocol_version"), QStringLiteral("1.0"));
    result.insert(QStringLiteral("bind"), bindAddressText());
    result.insert(QStringLiteral("ports"), ports);
    result.insert(QStringLiteral("http_endpoints"), httpEndpoints);
    result.insert(QStringLiteral("event_protocol"), QStringLiteral("JSON Lines; first line authenticates with token"));
    result.insert(QStringLiteral("instrument_protocol"), QStringLiteral("JSON Lines; first line authenticates with token"));
    result.insert(QStringLiteral("safety"), safety);
    return result;
}

void IntegrationGateway::publishEvent(const QString& type, const QJsonObject& payload) {
    QJsonObject event;
    event.insert(QStringLiteral("type"), type);
    event.insert(QStringLiteral("event_id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    event.insert(QStringLiteral("timestamp"), compactTimestamp());
    event.insert(QStringLiteral("payload"), payload);

    const auto clients = eventClients_.values();
    for (auto* socket : clients) {
        if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
            eventClients_.remove(socket);
            continue;
        }
        sendJsonLine(socket, event);
    }
}

void IntegrationGateway::acceptControlConnections() {
    while (controlServer_->hasPendingConnections()) {
        auto* socket = controlServer_->nextPendingConnection();
        controlBuffers_.insert(socket, {});
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { readControlSocket(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() { removeSocket(socket); });
    }
}

void IntegrationGateway::acceptEventConnections() {
    while (eventServer_->hasPendingConnections()) {
        auto* socket = eventServer_->nextPendingConnection();
        socket->setProperty("sdlAuthorized", false);
        jsonLineBuffers_.insert(socket, {});
        sendJsonLine(socket, QJsonObject{
            {QStringLiteral("type"), QStringLiteral("gateway.hello")},
            {QStringLiteral("channel"), QStringLiteral("events")},
            {QStringLiteral("protocol"), QStringLiteral("catalyst-sdl-events/1")},
            {QStringLiteral("auth"), QStringLiteral("send one JSON line containing {\"token\":\"...\"}")}
        });
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { readJsonLineSocket(socket, false); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() { removeSocket(socket); });
    }
}

void IntegrationGateway::acceptInstrumentConnections() {
    while (instrumentServer_->hasPendingConnections()) {
        auto* socket = instrumentServer_->nextPendingConnection();
        socket->setProperty("sdlAuthorized", false);
        jsonLineBuffers_.insert(socket, {});
        sendJsonLine(socket, QJsonObject{
            {QStringLiteral("type"), QStringLiteral("gateway.hello")},
            {QStringLiteral("channel"), QStringLiteral("instrument")},
            {QStringLiteral("protocol"), QStringLiteral("catalyst-sdl-instrument/1")},
            {QStringLiteral("mode"), QStringLiteral("telemetry_ingress_only")},
            {QStringLiteral("auth"), QStringLiteral("send one JSON line containing {\"token\":\"...\"}")}
        });
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { readJsonLineSocket(socket, true); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() { removeSocket(socket); });
    }
}

void IntegrationGateway::readControlSocket(QTcpSocket* socket) {
    if (!controlBuffers_.contains(socket)) return;
    auto& buffer = controlBuffers_[socket];
    buffer.append(socket->readAll());
    if (buffer.size() > kMaxHttpRequestBytes) {
        sendHttpJson(socket, 413, QJsonObject{{QStringLiteral("error"), QStringLiteral("request_too_large")}});
        return;
    }

    const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) return;

    const QByteArray headerBytes = buffer.left(headerEnd);
    const QList<QByteArray> lines = headerBytes.split('\n');
    if (lines.isEmpty()) {
        sendHttpJson(socket, 400, QJsonObject{{QStringLiteral("error"), QStringLiteral("invalid_http_request")}});
        return;
    }

    const QList<QByteArray> requestParts = lines.front().trimmed().split(' ');
    if (requestParts.size() < 2) {
        sendHttpJson(socket, 400, QJsonObject{{QStringLiteral("error"), QStringLiteral("invalid_request_line")}});
        return;
    }
    const QByteArray method = requestParts[0].trimmed().toUpper();
    const QByteArray path = requestParts[1].trimmed();

    QHash<QByteArray, QByteArray> headers;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines[i].trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0) continue;
        headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }

    bool contentLengthOk = true;
    qint64 contentLength = 0;
    if (headers.contains(QByteArrayLiteral("content-length"))) {
        contentLength = headers.value(QByteArrayLiteral("content-length")).toLongLong(&contentLengthOk);
    }
    if (!contentLengthOk || contentLength < 0 || contentLength > kMaxHttpRequestBytes) {
        sendHttpJson(socket, 400, QJsonObject{{QStringLiteral("error"), QStringLiteral("invalid_content_length")}});
        return;
    }

    const qsizetype bodyStart = headerEnd + 4;
    if (buffer.size() < bodyStart + contentLength) return;
    const QByteArray body = buffer.mid(bodyStart, contentLength);

    if (method == QByteArrayLiteral("GET") && path == QByteArrayLiteral("/v1/health")) {
        sendHttpJson(socket, 200, QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("service"), QStringLiteral("catalyst-longevity-sdl-gateway")},
            {QStringLiteral("running"), isRunning()},
            {QStringLiteral("timestamp"), compactTimestamp()}
        });
        return;
    }

    if (method == QByteArrayLiteral("GET") && path == QByteArrayLiteral("/v1/capabilities")) {
        sendHttpJson(socket, 200, capabilities());
        return;
    }

    if (method == QByteArrayLiteral("GET") && path == QByteArrayLiteral("/v1/pairing")) {
        if (!bindAddress_.isLoopback()) {
            sendHttpJson(socket, 403, QJsonObject{
                {QStringLiteral("error"), QStringLiteral("pairing_endpoint_loopback_only")},
                {QStringLiteral("hint"), QStringLiteral("For LAN use set CATALYST_SDL_TOKEN explicitly.")}
            });
            return;
        }
        sendHttpJson(socket, 200, QJsonObject{
            {QStringLiteral("token"), accessToken_},
            {QStringLiteral("scope"), QStringLiteral("current_application_session")},
            {QStringLiteral("warning"), QStringLiteral("Treat this token as a local secret.")}
        });
        return;
    }

    const QByteArray authorization = headers.value(QByteArrayLiteral("authorization"));
    const QByteArray bearerPrefix = QByteArrayLiteral("Bearer ");
    const QString suppliedToken = authorization.startsWith(bearerPrefix)
        ? QString::fromUtf8(authorization.mid(bearerPrefix.size())).trimmed()
        : QString();
    if (!tokenMatches(suppliedToken)) {
        sendHttpJson(socket, 401, QJsonObject{
            {QStringLiteral("error"), QStringLiteral("invalid_or_missing_bearer_token")}
        });
        return;
    }

    if (method == QByteArrayLiteral("GET") && path == QByteArrayLiteral("/v1/state")) {
        sendHttpJson(socket, 200, gatewayState());
        return;
    }

    if (method != QByteArrayLiteral("POST")) {
        sendHttpJson(socket, 405, QJsonObject{{QStringLiteral("error"), QStringLiteral("method_not_allowed")}});
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        sendHttpJson(socket, 400, QJsonObject{
            {QStringLiteral("error"), QStringLiteral("body_must_be_json_object")},
            {QStringLiteral("detail"), parseError.errorString()}
        });
        return;
    }
    QJsonObject envelope = document.object();
    envelope.insert(QStringLiteral("gateway_received_at"), compactTimestamp());

    if (path == QByteArrayLiteral("/v1/jobs")) {
        const QString requestId = envelope.value(QStringLiteral("id")).toString().trimmed().isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : envelope.value(QStringLiteral("id")).toString();
        envelope.insert(QStringLiteral("id"), requestId);
        envelope.insert(QStringLiteral("execution_enabled"), false);
        emit jobEnvelopeReceived(envelope);
        publishEvent(QStringLiteral("job.received"), QJsonObject{
            {QStringLiteral("id"), requestId},
            {QStringLiteral("execution_enabled"), false}
        });
        sendHttpJson(socket, 202, QJsonObject{
            {QStringLiteral("id"), requestId},
            {QStringLiteral("status"), QStringLiteral("received")},
            {QStringLiteral("execution_enabled"), false},
            {QStringLiteral("next_step"), QStringLiteral("connect an approved hardware adapter before enabling actuation")}
        });
        return;
    }

    if (path == QByteArrayLiteral("/v1/results")) {
        const QString resultId = envelope.value(QStringLiteral("id")).toString().trimmed().isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : envelope.value(QStringLiteral("id")).toString();
        envelope.insert(QStringLiteral("id"), resultId);
        emit resultEnvelopeReceived(envelope);
        publishEvent(QStringLiteral("result.received"), QJsonObject{{QStringLiteral("id"), resultId}});
        sendHttpJson(socket, 202, QJsonObject{
            {QStringLiteral("id"), resultId},
            {QStringLiteral("status"), QStringLiteral("received_for_integration")},
            {QStringLiteral("project_mutation"), false}
        });
        return;
    }

    if (path == QByteArrayLiteral("/v1/events")) {
        const QString type = envelope.value(QStringLiteral("type")).toString().trimmed();
        if (type.isEmpty()) {
            sendHttpJson(socket, 400, QJsonObject{{QStringLiteral("error"), QStringLiteral("event_type_required")}});
            return;
        }
        QJsonObject payload = envelope.value(QStringLiteral("payload")).toObject();
        publishEvent(type, payload);
        sendHttpJson(socket, 202, QJsonObject{
            {QStringLiteral("status"), QStringLiteral("published")},
            {QStringLiteral("type"), type}
        });
        return;
    }

    sendHttpJson(socket, 404, QJsonObject{{QStringLiteral("error"), QStringLiteral("endpoint_not_found")}});
}

void IntegrationGateway::readJsonLineSocket(QTcpSocket* socket, bool instrumentChannel) {
    if (!jsonLineBuffers_.contains(socket)) return;
    auto& buffer = jsonLineBuffers_[socket];
    buffer.append(socket->readAll());
    if (buffer.size() > kMaxJsonLineBytes) {
        sendJsonLine(socket, QJsonObject{{QStringLiteral("error"), QStringLiteral("json_line_buffer_too_large")}});
        socket->disconnectFromHost();
        return;
    }

    while (true) {
        const qsizetype newline = buffer.indexOf('\n');
        if (newline < 0) break;
        QByteArray line = buffer.left(newline).trimmed();
        buffer.remove(0, newline + 1);
        if (line.isEmpty()) continue;

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            sendJsonLine(socket, QJsonObject{
                {QStringLiteral("error"), QStringLiteral("invalid_json_line")},
                {QStringLiteral("detail"), parseError.errorString()}
            });
            continue;
        }

        const QJsonObject object = document.object();
        const QString channel = instrumentChannel ? QStringLiteral("instrument") : QStringLiteral("events");
        if (!socket->property("sdlAuthorized").toBool()) {
            authorizeJsonLine(socket, object, channel);
            continue;
        }

        if (!instrumentChannel) {
            if (object.value(QStringLiteral("type")).toString() == QStringLiteral("ping")) {
                sendJsonLine(socket, QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("pong")},
                    {QStringLiteral("timestamp"), compactTimestamp()}
                });
            }
            continue;
        }

        QJsonObject message = object;
        message.insert(QStringLiteral("gateway_received_at"), compactTimestamp());
        emit instrumentMessageReceived(message);
        publishEvent(QStringLiteral("instrument.message"), QJsonObject{
            {QStringLiteral("source"), message.value(QStringLiteral("source"))},
            {QStringLiteral("type"), message.value(QStringLiteral("type"))}
        });
        sendJsonLine(socket, QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("status"), QStringLiteral("telemetry_received")},
            {QStringLiteral("hardware_command_executed"), false}
        });
    }
}

void IntegrationGateway::removeSocket(QTcpSocket* socket) {
    controlBuffers_.remove(socket);
    jsonLineBuffers_.remove(socket);
    eventClients_.remove(socket);
    socket->deleteLater();
}

bool IntegrationGateway::tokenMatches(const QString& supplied) const {
    return !accessToken_.isEmpty() && supplied == accessToken_;
}

bool IntegrationGateway::authorizeJsonLine(QTcpSocket* socket, const QJsonObject& object, const QString& channel) {
    if (socket->property("sdlAuthorized").toBool()) return true;
    if (!tokenMatches(object.value(QStringLiteral("token")).toString())) {
        sendJsonLine(socket, QJsonObject{{QStringLiteral("error"), QStringLiteral("authentication_failed")}});
        socket->disconnectFromHost();
        return false;
    }

    socket->setProperty("sdlAuthorized", true);
    if (channel == QStringLiteral("events")) eventClients_.insert(socket);
    sendJsonLine(socket, QJsonObject{
        {QStringLiteral("ok"), true},
        {QStringLiteral("type"), QStringLiteral("gateway.authenticated")},
        {QStringLiteral("channel"), channel},
        {QStringLiteral("timestamp"), compactTimestamp()}
    });
    return true;
}

void IntegrationGateway::sendJsonLine(QTcpSocket* socket, const QJsonObject& object) {
    if (!socket || socket->state() == QAbstractSocket::UnconnectedState) return;
    socket->write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    socket->write("\n");
}

void IntegrationGateway::sendHttpJson(QTcpSocket* socket, int statusCode, const QJsonObject& object) {
    if (!socket) return;
    const QByteArray body = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QByteArray response;
    response += QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(statusCode) + QByteArrayLiteral(" ") + statusReason(statusCode) + QByteArrayLiteral("\r\n");
    response += QByteArrayLiteral("Content-Type: application/json; charset=utf-8\r\n");
    response += QByteArrayLiteral("Cache-Control: no-store\r\n");
    response += QByteArrayLiteral("Connection: close\r\n");
    response += QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n");
    response += body;
    socket->write(response);
    socket->disconnectFromHost();
}

QJsonObject IntegrationGateway::gatewayState() const {
    return QJsonObject{
        {QStringLiteral("running"), isRunning()},
        {QStringLiteral("bind"), bindAddressText()},
        {QStringLiteral("control_port"), static_cast<int>(controlPort())},
        {QStringLiteral("event_port"), static_cast<int>(eventPort())},
        {QStringLiteral("instrument_port"), static_cast<int>(instrumentPort())},
        {QStringLiteral("event_clients"), eventClients_.size()},
        {QStringLiteral("hardware_actuation"), false},
        {QStringLiteral("job_execution"), QStringLiteral("not_enabled")},
        {QStringLiteral("timestamp"), compactTimestamp()}
    };
}

} // namespace catalyst
