from pathlib import Path

p = Path('bear_agent_pet/client/src/PetWindowV7.cpp')
s = p.read_text(encoding='utf-8')


def replace_once(old: str, new: str) -> None:
    global s
    count = s.count(old)
    if count != 1:
        raise SystemExit(f'expected exactly one match, found {count}: {old[:100]!r}')
    s = s.replace(old, new, 1)

replace_once(
'''    else if(chosen==manualFriendCode) configureConnection();
''',
'''    else if(chosen==manualFriendCode) configureRecoveryConnection();
''',
)

replace_once(
'''void PetWindow::configureConnection(){
    QSettings s; bool ok=false;
    const QUrl endpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
    if(!endpoint.isValid() || endpoint.scheme().toLower()!="wss" || endpoint.host().isEmpty()) {
        QMessageBox::warning(this,"Tony",uiText("The server address in Settings is invalid.","设置中的服务器地址无效。"));
        return;
    }
    const QString code=QInputDialog::getText(this,uiText("Recovery connection","恢复连接"),uiText("Recovery friend code:","恢复好友码："),QLineEdit::Normal,{},&ok);
''',
'''void PetWindow::configureConnection(){
    startAutomaticPairing();
}

void PetWindow::configureRecoveryConnection(){
    QSettings s; bool ok=false;
    const QUrl endpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
    if(!endpoint.isValid() || endpoint.scheme().toLower()!="wss" || endpoint.host().isEmpty()) {
        QMessageBox::warning(this,"Tony",uiText("The server address in Settings is invalid.","设置中的服务器地址无效。"));
        return;
    }
    const QString code=QInputDialog::getText(this,uiText("Recovery connection","恢复连接"),uiText("Recovery friend code:","恢复好友码："),QLineEdit::Normal,{},&ok);
''',
)

p.write_text(s, encoding='utf-8')

main = Path('bear_agent_pet/client/src/main.cpp')
m = main.read_text(encoding='utf-8')
old = '''    // First launch stays non-modal. PetWindow already shows a short connection\n    // hint; the friend-code dialog opens only when the user explicitly chooses\n    // Connect to Tony. This keeps the pet visible instead of covering it at startup.\n'''
new = '''    // First launch stays non-modal. An unpaired PetWindow automatically exposes\n    // a short-lived device connection code after Tony is visibly on the desktop.\n    // Settings/reconnect uses the same device-code flow; friend code is recovery-only.\n'''
if old not in m:
    raise SystemExit('main.cpp onboarding comment anchor not found')
main.write_text(m.replace(old, new, 1), encoding='utf-8')

print('TONY_V096_CONNECTION_UX_PATCH=PASS')
