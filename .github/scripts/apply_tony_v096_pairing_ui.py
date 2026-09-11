from pathlib import Path

p = Path('bear_agent_pet/client/src/PetWindowV7.cpp')
s = p.read_text(encoding='utf-8')


def replace_once(old: str, new: str) -> None:
    global s
    count = s.count(old)
    if count != 1:
        raise SystemExit(f'expected exactly one match, found {count}: {old[:90]!r}')
    s = s.replace(old, new, 1)

replace_once(
    '#include <QApplication>\n#include <QContextMenuEvent>\n',
    '#include <QApplication>\n#include <QClipboard>\n#include <QContextMenuEvent>\n#include <QDateTime>\n',
)

replace_once(
'''    connect(&agent_, &AgentClient::connectionChanged, this, [this](bool connected){
        if(!connected) {
            agentState_="idle";
        } else if(agentState_=="idle" && !actionTimer_.isActive()) {
            emotion_="friendly";
            setAction(Action::Wave,900);
        }
        tray_.setToolTip(connected ? "Tony · connected" : "Tony · waiting for server");
    });
    connect(&agent_, &AgentClient::paired, this,
''',
'''    connect(&agent_, &AgentClient::connectionChanged, this, [this](bool connected){
        if(!connected) {
            agentState_="idle";
        } else if(agentState_=="idle" && !actionTimer_.isActive()) {
            emotion_="friendly";
            setAction(Action::Wave,900);
        }
        tray_.setToolTip(connected ? "Tony · connected" : "Tony · waiting for server");
    });
    connect(&agent_, &AgentClient::pairingCodeReady, this,
            [this](const QString &code, qint64 expiresAt){
        pairingCode_=code;
        pairingCodeExpiresAt_=expiresAt;
        emotion_="curious";
        setAction(Action::Think,0);
        tray_.setToolTip(QString("Tony · connection code · %1").arg(code));
        showCurrentPairingCode(false);
    });
    connect(&agent_, &AgentClient::paired, this,
''',
)

replace_once(
'''        s.setValue("agent/device_id",deviceId);
        if(endpoint.host()!="127.0.0.1" && endpoint.host()!="localhost") tunnel_.stop();
''',
'''        s.setValue("agent/device_id",deviceId);
        pairingCode_.clear();
        pairingCodeExpiresAt_=0;
        if(endpoint.host()!="127.0.0.1" && endpoint.host()!="localhost") tunnel_.stop();
''',
)

replace_once(
'''    connect(&agent_, &AgentClient::pairingFailed, this, [this](const QString &text){
        emotion_="worried";
        tray_.setToolTip("Tony · pairing failed");
        showBubble(text,6500);
    });
''',
'''    connect(&agent_, &AgentClient::pairingFailed, this, [this](const QString &text){
        pairingCode_.clear();
        pairingCodeExpiresAt_=0;
        emotion_="worried";
        tray_.setToolTip("Tony · pairing failed");
        showBubble(text,6500);
    });
''',
)

replace_once(
'''    } else {
        tray_.setToolTip("Tony · not paired");
        if(visualTestAction.isEmpty())
            showBubble(uiText("Hi Paula. Right-click me and choose Connect to Tony.","嗨 Paula。右键点我，然后选择“连接 Tony”。"),6500);
    }
''',
'''    } else {
        tray_.setToolTip("Tony · preparing connection code");
        if(visualTestAction.isEmpty()) {
            showBubble(uiText("Creating a secure connection code…","正在生成安全连接码…"),0);
            QTimer::singleShot(700,this,&PetWindow::startAutomaticPairing);
        }
    }
''',
)

replace_once(
'''    auto pair=m.addAction(agent_.connected() ? uiText("Reconnect / pair another computer…","重新连接 / 配对其他电脑…") : uiText("Connect to Tony…","连接 Tony…"));

    auto *settings=m.addMenu(uiText("Settings","设置"));
''',
'''    auto pair=m.addAction(agent_.connected() ? uiText("Generate code for another computer…","为另一台电脑生成连接码…") : uiText("Show / refresh connection code…","显示 / 刷新连接码…"));
    QAction *copyPairCode=nullptr;
    if(!pairingCode_.isEmpty()) copyPairCode=m.addAction(uiText("Copy connection code","复制连接码"));

    auto *settings=m.addMenu(uiText("Settings","设置"));
''',
)

replace_once(
'''    auto *connectionMenu=settings->addMenu(uiText("Connection","连接"));
    auto serverAddress=connectionMenu->addAction(uiText("Server address…","服务器地址…"));
    auto localSsh=connectionMenu->addAction(uiText("Use local SSH tunnel (advanced)","使用本机 SSH 隧道（高级）"));
''',
'''    auto *connectionMenu=settings->addMenu(uiText("Connection","连接"));
    auto serverAddress=connectionMenu->addAction(uiText("Server address…","服务器地址…"));
    auto manualFriendCode=connectionMenu->addAction(uiText("Enter recovery friend code…","输入恢复好友码…"));
    auto localSsh=connectionMenu->addAction(uiText("Use local SSH tunnel (advanced)","使用本机 SSH 隧道（高级）"));
''',
)

replace_once(
'''    else if(chosen==pair) configureConnection();
    else if(chosen==english || chosen==chinese) {
''',
'''    else if(chosen==pair) startAutomaticPairing();
    else if(copyPairCode && chosen==copyPairCode) showCurrentPairingCode(true);
    else if(chosen==english || chosen==chinese) {
''',
)

replace_once(
'''    else if(chosen==localSsh) useLocalSshConnection();
''',
'''    else if(chosen==manualFriendCode) configureConnection();
    else if(chosen==localSsh) useLocalSshConnection();
''',
)

replace_once(
'''        showBubble(uiText("Tony is not connected yet. Right-click me and choose Connect to Tony.\\n\\n", "Tony 还没有连接。右键点我并选择“连接 Tony”。\\n\\n")+prompt,6500);
''',
'''        if(pairingCode_.isEmpty()) startAutomaticPairing();
        else showCurrentPairingCode(false);
''',
)

replace_once(
'''void PetWindow::hugTony(){ markInteraction(); behavior_.onHugged(); emotion_="happy"; setAction(Action::Hug,2600); showBubble(uiText("Got you. Tony cuddles closer.","抱到啦。Tony 开心地靠近了一点。"),4300); }

void PetWindow::configureConnection(){
''',
'''void PetWindow::hugTony(){ markInteraction(); behavior_.onHugged(); emotion_="happy"; setAction(Action::Hug,2600); showBubble(uiText("Got you. Tony cuddles closer.","抱到啦。Tony 开心地靠近了一点。"),4300); }

void PetWindow::startAutomaticPairing(){
    QSettings s;
    const QUrl endpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
    if(!endpoint.isValid() || endpoint.scheme().toLower()!="wss" || endpoint.host().isEmpty()) {
        QMessageBox::warning(this,"Tony",uiText("The server address in Settings is invalid.","设置中的服务器地址无效。"));
        return;
    }
    const QString deviceName=QSysInfo::machineHostName().isEmpty() ? QString("Tony desktop") : QSysInfo::machineHostName();
    s.setValue("agent/url",endpoint);
    s.setValue("connection/prefer_local_ssh",false);
    pairingCode_.clear();
    pairingCodeExpiresAt_=0;
    emotion_="curious";
    setAction(Action::Think,0);
    tray_.setToolTip("Tony · requesting connection code");
    showBubble(uiText("Creating a secure connection code…","正在生成安全连接码…"),0);
    agent_.requestDevicePairing(endpoint,deviceName);
}

void PetWindow::showCurrentPairingCode(bool copyToClipboard){
    if(pairingCode_.isEmpty()) {
        startAutomaticPairing();
        return;
    }
    if(copyToClipboard) QApplication::clipboard()->setText(pairingCode_);
    const qint64 seconds=qMax<qint64>(0,pairingCodeExpiresAt_-QDateTime::currentSecsSinceEpoch());
    const int minutes=qMax(1,static_cast<int>((seconds+59)/60));
    const QString en=QString("Connection code: %1\\nTell this code to the Tony Agent/server owner to approve this computer.\\nValid for about %2 min.%3")
        .arg(pairingCode_).arg(minutes).arg(copyToClipboard ? "\\nCopied to clipboard." : "");
    const QString zh=QString("连接码：%1\\n把这个码告诉服务器上的 Tony Agent / 管理员，让它批准这台电脑。\\n约 %2 分钟内有效。%3")
        .arg(pairingCode_).arg(minutes).arg(copyToClipboard ? "\\n已复制到剪贴板。" : "");
    showBubble(uiText(en,zh),0);
}

void PetWindow::configureConnection(){
''',
)

replace_once(
'''    const QString code=QInputDialog::getText(this,uiText("Connect to Tony","连接 Tony"),uiText("Friend code:","好友码："),QLineEdit::Normal,{},&ok);
''',
'''    const QString code=QInputDialog::getText(this,uiText("Recovery connection","恢复连接"),uiText("Recovery friend code:","恢复好友码："),QLineEdit::Normal,{},&ok);
''',
)

p.write_text(s, encoding='utf-8')

version = Path('bear_agent_pet/client/VERSION')
version.write_text('0.9.6\n', encoding='utf-8')
print('TONY_V096_PAIRING_UI_PATCH=PASS')
