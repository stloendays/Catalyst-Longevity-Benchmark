from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_once(path: str, old: str, new: str) -> None:
    p = ROOT / path
    text = p.read_text(encoding="utf-8")
    if new in text:
        print(f"already patched: {path}")
        return
    if old not in text:
        raise SystemExit(f"guard not found in {path}: {old[:100]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"patched: {path}")


# Embed the generated Tony app icon in the Windows executable.
replace_once(
    "bear_agent_pet/client/src/TonyVersion.rc.in",
    "#include <windows.h>\n\nVS_VERSION_INFO VERSIONINFO",
    "#include <windows.h>\n\nIDI_TONY_APP_ICON ICON \"@CMAKE_CURRENT_SOURCE_DIR@/assets/tony-app.ico\"\n\nVS_VERSION_INFO VERSIONINFO",
)

# Use the same icon at runtime for the window/taskbar.
replace_once(
    "bear_agent_pet/client/src/main.cpp",
    "#include <QEvent>\n#include <QKeySequence>",
    "#include <QEvent>\n#include <QIcon>\n#include <QKeySequence>",
)
replace_once(
    "bear_agent_pet/client/src/main.cpp",
    "    QCoreApplication::setApplicationVersion(QStringLiteral(TONY_APP_VERSION));\n    app.setQuitOnLastWindowClosed(false);",
    "    QCoreApplication::setApplicationVersion(QStringLiteral(TONY_APP_VERSION));\n    const QIcon appIcon(QCoreApplication::applicationDirPath()+QStringLiteral(\"/assets/tony-app.ico\"));\n    if(!appIcon.isNull()) app.setWindowIcon(appIcon);\n    app.setQuitOnLastWindowClosed(false);",
)

# Use the application icon for the frameless pet window and tray icon.
replace_once(
    "bear_agent_pet/client/src/PetWindowV7.cpp",
    "    loadAssets();\n    restorePosition();",
    "    loadAssets();\n    setWindowIcon(QApplication::windowIcon());\n    restorePosition();",
)
replace_once(
    "bear_agent_pet/client/src/PetWindowV7.cpp",
    "    tray_.setToolTip(\"Tony · Desktop Agent\");\n    tray_.setVisible(true);",
    "    if(!QApplication::windowIcon().isNull()) tray_.setIcon(QApplication::windowIcon());\n    tray_.setToolTip(\"Tony · Desktop Agent\");\n    tray_.setVisible(true);",
)
replace_once(
    "bear_agent_pet/client/src/PetWindowV7.cpp",
    "    connect(&agent_, &AgentClient::pairingCodeReady, this,",
    "    connect(&agent_, &AgentClient::connectionStageChanged, this, [this](const QString &stage){\n        if(stage==QStringLiteral(\"connecting\"))\n            tray_.setToolTip(uiText(\"Tony · connecting securely\",\"Tony · 正在安全连接\"));\n        else if(stage==QStringLiteral(\"retrying\"))\n            tray_.setToolTip(uiText(\"Tony · retrying connection\",\"Tony · 正在重试连接\"));\n        else if(stage==QStringLiteral(\"pairing_request\"))\n            tray_.setToolTip(uiText(\"Tony · checking public server\",\"Tony · 正在检查公网服务器\"));\n        else if(stage==QStringLiteral(\"approval_required\"))\n            tray_.setToolTip(uiText(\"Tony · waiting for owner approval\",\"Tony · 等待服务器批准\"));\n        else if(stage==QStringLiteral(\"connected\"))\n            tray_.setToolTip(uiText(\"Tony · connected\",\"Tony · 已连接\"));\n    });\n    connect(&agent_, &AgentClient::pairingCodeReady, this,",
)
replace_once(
    "bear_agent_pet/client/src/PetWindowV7.cpp",
    "        showBubble(\"Tony couldn't finish that: \" + text.left(260),7000);",
    "        showBubble(uiText(\"Tony couldn't finish that: \",\"Tony 连接或执行失败：\") + text.left(320),8000);",
)

# Clarify the settings UI so connection testing is discoverable.
replace_once(
    "bear_agent_pet/client/src/SettingsDialog.cpp",
    "Tony reconnects automatically with the device token stored on this computer. You normally do not need to sign in again.",
    "Tony reconnects automatically with the device token stored on this computer. Use the button below to test the secure server path or create a fresh pairing code.",
)
replace_once(
    "bear_agent_pet/client/src/SettingsDialog.cpp",
    "Tony 会使用保存在这台电脑上的设备令牌自动重连，通常不需要再次登录。",
    "Tony 会使用保存在这台电脑上的设备令牌自动重连。可使用下方按钮测试安全连接，或重新生成配对码。",
)
replace_once(
    "bear_agent_pet/client/src/SettingsDialog.cpp",
    "Pair / reconnect this computer…",
    "Test connection / pair this computer…",
)
replace_once(
    "bear_agent_pet/client/src/SettingsDialog.cpp",
    "配对 / 重新连接这台电脑…",
    "测试连接 / 配对这台电脑…",
)

# Give the installer the same visual identity as the executable.
replace_once(
    "bear_agent_pet/client/installer/TonyDesktopPet.iss",
    "WizardStyle=modern\nArchitecturesAllowed=x64compatible",
    "WizardStyle=modern\nSetupIconFile=..\\assets\\tony-app.ico\nUninstallDisplayIcon={app}\\TonyDesktopPet.exe\nArchitecturesAllowed=x64compatible",
)

# Connection state and retry bookkeeping.
replace_once(
    "bear_agent_pet/client/src/AgentClient.h",
    "    void connectionChanged(bool connected);\n    void pairingCodeReady",
    "    void connectionChanged(bool connected);\n    void connectionStageChanged(const QString &stage);\n    void pairingCodeReady",
)
replace_once(
    "bear_agent_pet/client/src/AgentClient.h",
    "    void sendClientHello();\n\n    QWebSocket socket_;",
    "    void sendClientHello();\n    void scheduleReconnect();\n\n    QWebSocket socket_;",
)
replace_once(
    "bear_agent_pet/client/src/AgentClient.h",
    "    QTimer reconnectTimer_;\n    QTimer pairingPollTimer_;",
    "    QTimer reconnectTimer_;\n    QTimer connectWatchdog_;\n    QTimer pairingPollTimer_;\n    int reconnectDelayMs_{1000};",
)

# Human-readable transport diagnosis. This deliberately calls out TCP 443 for
# public WSS/HTTPS because that is the failure mode we can actually act on.
replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "    return url;\n}\n}\n\nAgentClient::AgentClient",
    "    return url;\n}\n\nQString friendlyTransportMessage(const QString &raw, const QUrl &url, bool zh) {\n    const QString lower=raw.toLower();\n    const bool secure=url.scheme().compare(QStringLiteral(\"wss\"),Qt::CaseInsensitive)==0 ||\n                      url.scheme().compare(QStringLiteral(\"https\"),Qt::CaseInsensitive)==0;\n    if(lower.contains(QStringLiteral(\"timed out\")) || lower.contains(QStringLiteral(\"timeout\"))) {\n        if(secure) return zh ? QStringLiteral(\"无法连接 Tony 公网入口（超时）。请检查云服务器安全组是否放行 TCP 443。\")\n                             : QStringLiteral(\"Tony's public endpoint timed out. Check that the cloud security group allows inbound TCP 443.\");\n        return zh ? QStringLiteral(\"连接 Tony 超时。\") : QStringLiteral(\"The Tony connection timed out.\");\n    }\n    if(lower.contains(QStringLiteral(\"host not found\")) || lower.contains(QStringLiteral(\"name or service\")))\n        return zh ? QStringLiteral(\"无法解析 Tony 服务器域名，请检查网络或服务器地址。\")\n                  : QStringLiteral(\"Tony's server name could not be resolved. Check the network or server address.\");\n    if(lower.contains(QStringLiteral(\"ssl\")) || lower.contains(QStringLiteral(\"tls\")) || lower.contains(QStringLiteral(\"handshake\")))\n        return zh ? QStringLiteral(\"Tony 的 TLS 安全连接失败。请检查 HTTPS 证书以及 TCP 443 是否可达。\")\n                  : QStringLiteral(\"Tony's TLS handshake failed. Check the HTTPS certificate and TCP 443 reachability.\");\n    if(lower.contains(QStringLiteral(\"refused\")))\n        return zh ? QStringLiteral(\"Tony 服务器拒绝连接，请检查反向代理和网关服务。\")\n                  : QStringLiteral(\"Tony's server refused the connection. Check the reverse proxy and gateway service.\");\n    return raw.trimmed().isEmpty()\n        ? (zh ? QStringLiteral(\"Tony 网络连接失败。\") : QStringLiteral(\"Tony's network connection failed.\"))\n        : raw.trimmed();\n}\n}\n\nAgentClient::AgentClient",
)

old_ctor = '''AgentClient::AgentClient(QObject *parent): QObject(parent) {
    reconnectTimer_.setInterval(3000);
    reconnectTimer_.setSingleShot(false);
    connect(&reconnectTimer_, &QTimer::timeout, this, &AgentClient::reconnect);

    pairingPollTimer_.setInterval(2000);
    pairingPollTimer_.setSingleShot(false);
    connect(&pairingPollTimer_, &QTimer::timeout, this, &AgentClient::pollDevicePairing);

    connect(&socket_, &QWebSocket::connected, this, [this]{
        reconnectTimer_.stop();
        outageReported_=false;
        sendClientHello();
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
'''
new_ctor = '''AgentClient::AgentClient(QObject *parent): QObject(parent) {
    reconnectTimer_.setSingleShot(true);
    connect(&reconnectTimer_, &QTimer::timeout, this, &AgentClient::reconnect);

    connectWatchdog_.setSingleShot(true);
    connect(&connectWatchdog_, &QTimer::timeout, this, [this]{
        if(socket_.state()!=QAbstractSocket::ConnectingState) return;
        const QString detail=friendlyTransportMessage(
            language_=="zh" ? QStringLiteral("连接超时") : QStringLiteral("connection timed out"),
            endpoint_,language_=="zh");
        socket_.abort();
        if(!outageReported_) {
            outageReported_=true;
            emit errorMessage(detail);
        }
        emit connectionStageChanged(QStringLiteral("retrying"));
        scheduleReconnect();
    });

    pairingPollTimer_.setInterval(2000);
    pairingPollTimer_.setSingleShot(false);
    connect(&pairingPollTimer_, &QTimer::timeout, this, &AgentClient::pollDevicePairing);

    connect(&socket_, &QWebSocket::connected, this, [this]{
        connectWatchdog_.stop();
        reconnectTimer_.stop();
        reconnectDelayMs_=1000;
        outageReported_=false;
        sendClientHello();
        emit connectionStageChanged(QStringLiteral("connected"));
        emit connectionChanged(true);
    });
    connect(&socket_, &QWebSocket::disconnected, this, [this]{
        connectWatchdog_.stop();
        emit connectionChanged(false);
        if(endpoint_.isValid() && !bearerToken_.isEmpty()) {
            emit connectionStageChanged(QStringLiteral("retrying"));
            scheduleReconnect();
        }
    });
    connect(&socket_, &QWebSocket::textMessageReceived, this, &AgentClient::onText);
    connect(&socket_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError){
        connectWatchdog_.stop();
        if(!outageReported_){
            outageReported_=true;
            emit errorMessage(friendlyTransportMessage(socket_.errorString(),endpoint_,language_=="zh"));
        }
        emit connectionStageChanged(QStringLiteral("retrying"));
        scheduleReconnect();
    });
}
'''
replace_once("bear_agent_pet/client/src/AgentClient.cpp", old_ctor, new_ctor)

replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "    outageReported_=false;\n    reconnectTimer_.stop();",
    "    outageReported_=false;\n    reconnectDelayMs_=1000;\n    connectWatchdog_.stop();\n    reconnectTimer_.stop();",
)

replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "    request.setHeader(QNetworkRequest::UserAgentHeader,tonyUserAgent());\n    const QJsonObject body{\n        {\"device_name\"",
    "    request.setHeader(QNetworkRequest::UserAgentHeader,tonyUserAgent());\n    request.setTransferTimeout(12000);\n    emit connectionStageChanged(QStringLiteral(\"pairing_request\"));\n    const QJsonObject body{\n        {\"device_name\"",
)
replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "    connect(reply,&QNetworkReply::finished,this,[this,reply]{\n        const QByteArray raw=reply->readAll();",
    "    connect(reply,&QNetworkReply::finished,this,[this,reply,requestUrl]{\n        const QByteArray raw=reply->readAll();",
)
replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "            QString detail=obj.value(\"detail\").toString();\n            if(detail.isEmpty()) detail=reply->errorString();\n            emit pairingFailed((language_==\"zh\" ? QString(\"无法生成连接码：\") : QString(\"Could not create a connection code: \"))+detail);",
    "            QString detail=obj.value(\"detail\").toString();\n            if(detail.isEmpty()) detail=friendlyTransportMessage(reply->errorString(),requestUrl,language_==\"zh\");\n            emit connectionStageChanged(QStringLiteral(\"offline\"));\n            emit pairingFailed((language_==\"zh\" ? QString(\"无法生成连接码：\") : QString(\"Could not create a connection code: \"))+detail);",
)
replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "        emit pairingCodeReady(code,pairingExpiresAt_);",
    "        emit connectionStageChanged(QStringLiteral(\"approval_required\"));\n        emit pairingCodeReady(code,pairingExpiresAt_);",
)

# Pair-status polling should not hang indefinitely when the public route disappears.
replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "    request.setHeader(QNetworkRequest::UserAgentHeader,tonyUserAgent());\n    const QJsonObject body{{\"request_id\",pairingRequestId_}};",
    "    request.setHeader(QNetworkRequest::UserAgentHeader,tonyUserAgent());\n    request.setTransferTimeout(8000);\n    const QJsonObject body{{\"request_id\",pairingRequestId_}};",
)
replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "        emit paired(token,deviceId,endpoint);\n        connectTo(endpoint,token);",
    "        emit connectionStageChanged(QStringLiteral(\"connecting\"));\n        emit paired(token,deviceId,endpoint);\n        connectTo(endpoint,token);",
)

# Recovery pairing also gets a bounded HTTPS timeout and actionable errors.
replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "    request.setHeader(QNetworkRequest::UserAgentHeader, tonyUserAgent());\n    const QJsonObject body{",
    "    request.setHeader(QNetworkRequest::UserAgentHeader, tonyUserAgent());\n    request.setTransferTimeout(12000);\n    const QJsonObject body{",
)
replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "    connect(reply,&QNetworkReply::finished,this,[this,reply,wsUrl]{",
    "    connect(reply,&QNetworkReply::finished,this,[this,reply,wsUrl,pairUrl]{",
)
replace_once(
    "bear_agent_pet/client/src/AgentClient.cpp",
    "            QString detail=obj.value(\"detail\").toString();\n            if(detail.isEmpty()) detail=reply->errorString();\n            emit pairingFailed((language_==\"zh\" ? QString(\"配对失败：\") : QString(\"Pairing failed: \"))+detail);",
    "            QString detail=obj.value(\"detail\").toString();\n            if(detail.isEmpty()) detail=friendlyTransportMessage(reply->errorString(),pairUrl,language_==\"zh\");\n            emit pairingFailed((language_==\"zh\" ? QString(\"配对失败：\") : QString(\"Pairing failed: \"))+detail);",
)

old_reconnect = '''void AgentClient::reconnect() {
    if(!endpoint_.isValid() || socket_.state()!=QAbstractSocket::UnconnectedState) return;
    QNetworkRequest request(endpoint_);
    request.setHeader(QNetworkRequest::UserAgentHeader, tonyUserAgent());
    if(!bearerToken_.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + bearerToken_.toUtf8());
    socket_.open(request);
}
'''
new_reconnect = '''void AgentClient::scheduleReconnect() {
    if(!endpoint_.isValid() || bearerToken_.isEmpty() || reconnectTimer_.isActive()) return;
    reconnectTimer_.start(reconnectDelayMs_);
    reconnectDelayMs_=qMin(reconnectDelayMs_*2,15000);
}

void AgentClient::reconnect() {
    if(!endpoint_.isValid() || bearerToken_.isEmpty() || socket_.state()!=QAbstractSocket::UnconnectedState) return;
    emit connectionStageChanged(QStringLiteral("connecting"));
    QNetworkRequest request(endpoint_);
    request.setHeader(QNetworkRequest::UserAgentHeader, tonyUserAgent());
    request.setRawHeader("Authorization", QByteArray("Bearer ") + bearerToken_.toUtf8());
    connectWatchdog_.start(12000);
    socket_.open(request);
}
'''
replace_once("bear_agent_pet/client/src/AgentClient.cpp", old_reconnect, new_reconnect)

print("TONY_V098_PATCH=READY")
