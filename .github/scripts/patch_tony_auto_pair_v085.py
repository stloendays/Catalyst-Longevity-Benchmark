from pathlib import Path

ROOT = Path('teddy_agent_pet/client')

def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'missing patch anchor: {label}')
    return text.replace(old, new, 1)

# AgentClient: add trusted SSH-tunnel bootstrap pairing.
p = ROOT / 'src/AgentClient.h'
s = p.read_text(encoding='utf-8')
s = replace_once(
    s,
    '    void pairAndConnect(const QUrl &wsUrl, const QString &pairingCode, const QString &deviceName);\n',
    '    void pairAndConnect(const QUrl &wsUrl, const QString &pairingCode, const QString &deviceName);\n'
    '    void pairViaTrustedTunnel(const QUrl &wsUrl, const QUrl &bootstrapUrl, const QString &deviceName);\n',
    'AgentClient.h trusted pair declaration',
)
p.write_text(s, encoding='utf-8')

p = ROOT / 'src/AgentClient.cpp'
s = p.read_text(encoding='utf-8')
anchor = '''void AgentClient::reconnect() {\n'''
method = r'''void AgentClient::pairViaTrustedTunnel(const QUrl &wsUrl, const QUrl &bootstrapUrl, const QString &deviceName) {
    const QString host=bootstrapUrl.host().trimmed().toLower();
    const bool loopback=(host=="127.0.0.1" || host=="localhost" || host=="::1");
    if(!bootstrapUrl.isValid() || !loopback || bootstrapUrl.scheme().toLower()!="http") {
        emit pairingFailed(language_=="zh" ? "安全自动配对地址无效。" : "The secure auto-pair address is invalid.");
        return;
    }

    QNetworkRequest request(bootstrapUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "TonyDesktopPet/0.8.5");
    const QJsonObject body{
        {"device_name",deviceName.trimmed().isEmpty() ? QString("Tony desktop") : deviceName.trimmed()}
    };
    auto *reply=network_.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[this,reply,wsUrl]{
        const QByteArray raw=reply->readAll();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto doc=QJsonDocument::fromJson(raw);
        const auto obj=doc.isObject() ? doc.object() : QJsonObject{};

        if(reply->error()!=QNetworkReply::NoError || status<200 || status>=300) {
            QString detail=obj.value("detail").toString();
            if(detail.isEmpty()) detail=reply->errorString();
            emit pairingFailed((language_=="zh" ? QString("自动验证失败：") : QString("Automatic verification failed: "))+detail);
            reply->deleteLater();
            return;
        }

        const QString token=obj.value("token").toString().trimmed();
        const QString deviceId=obj.value("device_id").toString().trimmed();
        if(token.isEmpty()) {
            emit pairingFailed(language_=="zh" ? "自动配对响应中没有设备令牌。" : "Automatic pairing returned no device token.");
            reply->deleteLater();
            return;
        }

        emit paired(token,deviceId,wsUrl);
        connectTo(wsUrl,token);
        reply->deleteLater();
    });
}

'''
s = replace_once(s, anchor, method + anchor, 'AgentClient.cpp trusted pair implementation')
s = s.replace('TonyDesktopPet/0.8"', 'TonyDesktopPet/0.8.5"')
s = s.replace('{"client_version","0.8.0"}', '{"client_version","0.8.5"}')
p.write_text(s, encoding='utf-8')

# SshTunnel: expose readiness and local forwarded port for secure bootstrap.
p = ROOT / 'src/SshTunnel.h'
s = p.read_text(encoding='utf-8')
s = replace_once(
    s,
    '    bool running() const;\n',
    '    bool running() const;\n    bool ready() const;\n    int localPort() const;\n',
    'SshTunnel.h readiness methods',
)
p.write_text(s, encoding='utf-8')

p = ROOT / 'src/SshTunnel.cpp'
s = p.read_text(encoding='utf-8')
s = replace_once(
    s,
    '''bool SshTunnel::running() const {\n    return process_.state()!=QProcess::NotRunning;\n}\n''',
    '''bool SshTunnel::running() const {\n    return process_.state()!=QProcess::NotRunning;\n}\n\nint SshTunnel::localPort() const {\n    return QSettings().value("ssh/local_port",18790).toInt();\n}\n\nbool SshTunnel::ready() const {\n    return localForwardReady(localPort());\n}\n''',
    'SshTunnel.cpp readiness methods',
)
p.write_text(s, encoding='utf-8')

# PetWindow declarations/state.
p = ROOT / 'src/PetWindow.h'
s = p.read_text(encoding='utf-8')
s = replace_once(
    s,
    '    void configureConnection();\n    void useLocalSshConnection();\n',
    '    void configureConnection();\n    void beginSecureOwnerPairing(bool friendFallbackOnFailure);\n'
    '    void trySecureOwnerBootstrap();\n    void showFriendCodeDialog();\n'
    '    void useLocalSshConnection();\n',
    'PetWindow.h secure pairing methods',
)
s = replace_once(
    s,
    '    QTimer desktopTimer_;\n',
    '    QTimer desktopTimer_;\n    QTimer ownerPairTimer_;\n',
    'PetWindow.h owner pair timer',
)
s = replace_once(
    s,
    '    bool hasWalkTarget_{false};\n',
    '    bool hasWalkTarget_{false};\n    bool ownerPairing_{false};\n    bool ownerPairFriendFallback_{false};\n    bool ownerBootstrapRequestSent_{false};\n',
    'PetWindow.h owner pair flags',
)
s = replace_once(
    s,
    '    int cursorStillTicks_{0};\n',
    '    int cursorStillTicks_{0};\n    int ownerPairAttempts_{0};\n',
    'PetWindow.h owner pair attempts',
)
p.write_text(s, encoding='utf-8')

# PetWindow behavior.
p = ROOT / 'src/PetWindowV7.cpp'
s = p.read_text(encoding='utf-8')

s = replace_once(
    s,
    '''    desktopTimer_.setInterval(1000);\n    connect(&desktopTimer_, &QTimer::timeout, this, &PetWindow::tickDesktop);\n    desktopTimer_.start();\n''',
    '''    desktopTimer_.setInterval(1000);\n    connect(&desktopTimer_, &QTimer::timeout, this, &PetWindow::tickDesktop);\n    desktopTimer_.start();\n\n    ownerPairTimer_.setInterval(700);\n    ownerPairTimer_.setSingleShot(false);\n    connect(&ownerPairTimer_, &QTimer::timeout, this, &PetWindow::trySecureOwnerBootstrap);\n''',
    'PetWindowV7 owner pair timer setup',
)

s = replace_once(
    s,
    '''    connect(&agent_, &AgentClient::paired, this,\n            [this](const QString &token, const QString &deviceId, const QUrl &endpoint){\n        QSettings s;\n''',
    '''    connect(&agent_, &AgentClient::paired, this,\n            [this](const QString &token, const QString &deviceId, const QUrl &endpoint){\n        ownerPairTimer_.stop();\n        ownerPairing_=false;\n        ownerPairFriendFallback_=false;\n        ownerBootstrapRequestSent_=false;\n        QSettings s;\n''',
    'PetWindowV7 paired reset',
)

old_failure = '''    connect(&agent_, &AgentClient::pairingFailed, this, [this](const QString &text){\n        emotion_="worried";\n        tray_.setToolTip("Tony · pairing failed");\n        showBubble(text,6500);\n    });\n'''
new_failure = '''    connect(&agent_, &AgentClient::pairingFailed, this, [this](const QString &text){\n        const bool automatic=ownerPairing_;\n        const bool openFriend=automatic && ownerPairFriendFallback_;\n        if(automatic) {\n            ownerPairTimer_.stop();\n            ownerPairing_=false;\n            ownerBootstrapRequestSent_=false;\n            tunnel_.stop();\n        }\n        emotion_="worried";\n        tray_.setToolTip("Tony · pairing failed");\n        if(openFriend) {\n            ownerPairFriendFallback_=false;\n            showFriendCodeDialog();\n        } else if(automatic) {\n            ownerPairFriendFallback_=false;\n            showBubble(uiText("I couldn't verify this computer through your SSH key. Right-click me and choose Connect to Tony to use a Friend code once.",\n                              "没能通过你的 SSH 身份自动验证这台电脑。右键点我并选择“连接 Tony”，好友码只需输入一次。"),7000);\n        } else {\n            showBubble(text,6500);\n        }\n    });\n'''
s = replace_once(s, old_failure, new_failure, 'PetWindowV7 pairing failure fallback')

old_startup = '''    } else {\n        tray_.setToolTip("Tony · not paired");\n        showBubble(uiText("Hi Paula. Right-click me and choose Connect to Tony.","嗨 Paula。右键点我，然后选择“连接 Tony”。"),6500);\n    }\n}\n'''
new_startup = '''    } else {\n        tray_.setToolTip("Tony · checking this computer");\n        showBubble(uiText("Hi Paula. I'm checking whether this is your trusted computer…",\n                          "嗨 Paula。我先看看这是不是你已经信任的电脑……"),4200);\n        QTimer::singleShot(800,this,[this]{ beginSecureOwnerPairing(false); });\n    }\n}\n'''
s = replace_once(s, old_startup, new_startup, 'PetWindowV7 startup auto pair')

start = s.index('void PetWindow::configureConnection(){')
end = s.index('\nvoid PetWindow::useLocalSshConnection(){', start)
new_config = r'''void PetWindow::configureConnection(){
    QSettings s;
    const QUrl endpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
    if(!endpoint.isValid() || endpoint.scheme().toLower()!="wss" || endpoint.host().isEmpty()) {
        QMessageBox::warning(this,"Tony",uiText("The server address in Settings is invalid.","设置中的服务器地址无效。"));
        return;
    }

    const QString token=unprotectSecret(s.value("agent/token","").toString());
    if(!token.isEmpty()) {
        s.setValue("connection/prefer_local_ssh",false);
        s.setValue("agent/url",endpoint);
        agent_.connectTo(endpoint,token);
        showBubble(uiText("Reconnecting with this computer's saved secure token…","正在使用这台电脑已保存的安全令牌重新连接……"),3600);
        return;
    }

    beginSecureOwnerPairing(true);
}

void PetWindow::beginSecureOwnerPairing(bool friendFallbackOnFailure){
    if(ownerPairing_) return;
    QSettings s;
    const QUrl endpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
    if(!endpoint.isValid() || endpoint.scheme().toLower()!="wss" || endpoint.host().isEmpty()) {
        if(friendFallbackOnFailure) showFriendCodeDialog();
        return;
    }

    ownerPairing_=true;
    ownerPairFriendFallback_=friendFallbackOnFailure;
    ownerBootstrapRequestSent_=false;
    ownerPairAttempts_=0;
    s.setValue("connection/prefer_local_ssh",false);
    tray_.setToolTip("Tony · verifying owner computer");
    emotion_="curious";
    setAction(Action::Think,0);
    showBubble(uiText("Checking your SSH identity so you don't need a Friend code…",
                      "正在通过你的 SSH 身份验证，这样就不用输入好友码了……"),0);
    tunnel_.start();
    ownerPairTimer_.start();
    QTimer::singleShot(120,this,&PetWindow::trySecureOwnerBootstrap);
}

void PetWindow::trySecureOwnerBootstrap(){
    if(!ownerPairing_ || ownerBootstrapRequestSent_) return;
    ++ownerPairAttempts_;

    if(tunnel_.ready()) {
        ownerPairTimer_.stop();
        ownerBootstrapRequestSent_=true;
        QSettings s;
        const QUrl publicEndpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
        QUrl bootstrap;
        bootstrap.setScheme("http");
        bootstrap.setHost("127.0.0.1");
        bootstrap.setPort(tunnel_.localPort());
        bootstrap.setPath("/pair/ssh-bootstrap");
        const QString deviceName=QSysInfo::machineHostName().isEmpty() ? QString("Tony desktop") : QSysInfo::machineHostName();
        tray_.setToolTip("Tony · secure owner pairing");
        agent_.pairViaTrustedTunnel(publicEndpoint,bootstrap,deviceName);
        return;
    }

    if(ownerPairAttempts_ < 12) return;

    const bool openFriend=ownerPairFriendFallback_;
    ownerPairTimer_.stop();
    ownerPairing_=false;
    ownerPairFriendFallback_=false;
    tunnel_.stop();
    if(openFriend) {
        showFriendCodeDialog();
    } else {
        tray_.setToolTip("Tony · not paired");
        emotion_="gentle";
        setAction(Action::Idle);
        showBubble(uiText("Automatic owner verification isn't available on this computer. Right-click me and choose Connect to Tony; the Friend code is only needed once.",
                          "这台电脑暂时无法自动验证。右键点我并选择“连接 Tony”；好友码只需要输入一次。"),7200);
    }
}

void PetWindow::showFriendCodeDialog(){
    QSettings s;
    bool ok=false;
    const QUrl endpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
    const QString code=QInputDialog::getText(
        this,
        uiText("One-time setup","首次连接"),
        uiText("Friend code (only needed once on this computer):","好友码（这台电脑只需输入一次）："),
        QLineEdit::Normal,{},&ok);
    if(!ok || code.trimmed().isEmpty()) {
        setAction(Action::Idle);
        return;
    }
    const QString deviceName=QSysInfo::machineHostName().isEmpty() ? QString("Tony desktop") : QSysInfo::machineHostName();
    s.setValue("connection/prefer_local_ssh",false);
    emotion_="curious";
    setAction(Action::Think,0);
    tray_.setToolTip("Tony · pairing…");
    showBubble(uiText("Connecting securely…","正在安全连接……"),0);
    agent_.pairAndConnect(endpoint,code,deviceName);
}
'''
s = s[:start] + new_config + s[end:]

p.write_text(s, encoding='utf-8')

# Version bump.
p = ROOT / 'CMakeLists.txt'
s = p.read_text(encoding='utf-8')
s = replace_once(s, 'project(TonyDesktopPet VERSION 0.8.4 LANGUAGES CXX)', 'project(TonyDesktopPet VERSION 0.8.5 LANGUAGES CXX)', 'CMake v0.8.5')
p.write_text(s, encoding='utf-8')

print('Tony v0.8.5 secure owner auto-pair patch applied')
