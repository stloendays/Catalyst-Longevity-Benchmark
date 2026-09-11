#include "PetWindow.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QSysInfo>
#include <QTime>
#include <QtMath>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {
QString protectSecret(const QString &plain) {
    if(plain.isEmpty()) return {};
#ifdef Q_OS_WIN
    const QByteArray inputBytes=plain.toUtf8();
    DATA_BLOB input{};
    input.pbData=reinterpret_cast<BYTE*>(const_cast<char*>(inputBytes.constData()));
    input.cbData=static_cast<DWORD>(inputBytes.size());
    DATA_BLOB output{};
    if(CryptProtectData(&input,L"Tony Desktop Pet",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)) {
        const QByteArray encrypted(reinterpret_cast<const char*>(output.pbData),static_cast<int>(output.cbData));
        LocalFree(output.pbData);
        return QStringLiteral("dpapi:")+QString::fromLatin1(encrypted.toBase64());
    }
#endif
    return plain;
}

QString unprotectSecret(const QString &stored) {
    if(stored.isEmpty()) return {};
#ifdef Q_OS_WIN
    if(stored.startsWith(QStringLiteral("dpapi:"))) {
        const QByteArray encrypted=QByteArray::fromBase64(stored.mid(6).toLatin1());
        DATA_BLOB input{};
        input.pbData=reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.constData()));
        input.cbData=static_cast<DWORD>(encrypted.size());
        DATA_BLOB output{};
        if(CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)) {
            const QByteArray plain(reinterpret_cast<const char*>(output.pbData),static_cast<int>(output.cbData));
            LocalFree(output.pbData);
            return QString::fromUtf8(plain);
        }
        return {};
    }
#endif
    return stored;
}
}

PetWindow::PetWindow(QWidget *parent): QWidget(parent), bubble_(nullptr) {
    setWindowTitle("Tony");
    setFixedSize(230,250);
    setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint|Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    loadAssets();
    restorePosition();

    animTimer_.setInterval(40);
    connect(&animTimer_, &QTimer::timeout, this, &PetWindow::tickAnimation);
    animTimer_.start();

    idleTimer_.setSingleShot(true);
    connect(&idleTimer_, &QTimer::timeout, this, &PetWindow::runIdleMoment);
    scheduleIdleMoment();

    actionTimer_.setSingleShot(true);
    connect(&actionTimer_, &QTimer::timeout, this, &PetWindow::restoreAgentAction);

    connect(&agent_, &AgentClient::stateChanged, this, [this](const QString &s){
        agentState_=s.trimmed().toLower();
        if(!actionTimer_.isActive()) restoreAgentAction();
    });
    connect(&agent_, &AgentClient::avatarAction, this,
            [this](const QString &action, const QString &emotion, int durationMs){
        emotion_=emotion;
        setAction(actionFromWire(action),durationMs);
        tray_.setToolTip(QString("Tony · %1 · %2").arg(action,emotion));
    });
    connect(&agent_, &AgentClient::textDelta, this, [this](const QString &t){
        answer_ += t;
        showBubble(answer_,0);
    });
    connect(&agent_, &AgentClient::answerFinished, this, [this]{
        if(!answer_.isEmpty()) showBubble(answer_,8000);
        if(!actionTimer_.isActive()) setAction(Action::Celebrate,1800);
    });
    connect(&agent_, &AgentClient::connectionChanged, this, [this](bool connected){
        if(!connected) {
            agentState_="idle";
        } else if(agentState_=="idle" && !actionTimer_.isActive()) {
            emotion_="friendly";
            setAction(Action::Wave,1500);
        }
        tray_.setToolTip(connected ? "Tony · connected" : "Tony · waiting for server");
    });
    connect(&agent_, &AgentClient::paired, this,
            [this](const QString &token, const QString &deviceId, const QUrl &endpoint){
        QSettings s;
        s.setValue("agent/url",endpoint);
        s.setValue("agent/token",protectSecret(token));
        s.setValue("agent/device_id",deviceId);
        if(endpoint.host()!="127.0.0.1" && endpoint.host()!="localhost") tunnel_.stop();
        emotion_="happy";
        setAction(Action::Celebrate,2400);
        tray_.setToolTip("Tony · paired · connecting");
        showBubble("配对成功。以后这台电脑可以直接连接 Tony，不需要保存服务器 SSH 私钥。",6500);
    });
    connect(&agent_, &AgentClient::pairingFailed, this, [this](const QString &text){
        emotion_="worried";
        tray_.setToolTip("Tony · pairing failed");
        showBubble(text,6500);
    });
    connect(&agent_, &AgentClient::errorMessage, this, [this](const QString &text){
        agentState_="error";
        emotion_="worried";
        setAction(Action::Think,2800);
        showBubble("Tony couldn't finish that: " + text.left(260),7000);
    });
    connect(&tunnel_, &SshTunnel::statusChanged, this, [this](const QString &status){
        if(!agent_.connected()) tray_.setToolTip("Tony · "+status);
        if(status.contains("unavailable",Qt::CaseInsensitive) || status.contains("waiting",Qt::CaseInsensitive)) {
            emotion_="worried";
            showBubble("SSH 连接没有准备好。你也可以右键 Tony → “连接 / 配对新设备…” 使用 WSS 配对。",6500);
        }
    });

    tray_.setToolTip("Tony · Desktop Agent");
    tray_.setVisible(true);
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r){
        if(r==QSystemTrayIcon::Trigger){ show(); raise(); }
    });

    QSettings s;
    const auto endpoint=s.value("agent/url","ws://127.0.0.1:18790/agent/ws").toUrl();
    const auto token=unprotectSecret(s.value("agent/token","").toString());
    if(endpoint.host()=="127.0.0.1" || endpoint.host()=="localhost") tunnel_.start();
    agent_.connectTo(endpoint,token);
}

PetWindow::~PetWindow() {
    bubble_.dismiss();
    tunnel_.stop();
}

void PetWindow::loadAssets() {
    const QString appDir=QCoreApplication::applicationDirPath();
    const QStringList fallbackCandidates{
        appDir+"/assets/tony.png",
        appDir+"/tony.png",
        QDir::currentPath()+"/assets/tony.png"
    };
    for(const auto &p:fallbackCandidates) {
        if(QFileInfo::exists(p) && pet_.load(p)) break;
    }

    const QStringList stateRoots{
        appDir+"/assets/states",
        QDir::currentPath()+"/assets/states"
    };
    const QStringList keys{
        "idle","working","walk","thinking","celebrate","sleep","shiver",
        "ask_hug","hug","blush","blush_wave","study","adjust_glasses","remove_glasses","wave"
    };
    for(const auto &key:keys) {
        for(const auto &root:stateRoots) {
            QPixmap sprite;
            const QString path=root+"/"+key+".png";
            if(QFileInfo::exists(path) && sprite.load(path)) {
                stateAssets_.insert(key,sprite);
                break;
            }
        }
    }

    const QStringList animationRoots{
        appDir+"/assets/animations",
        QDir::currentPath()+"/assets/animations"
    };
    const QStringList animatedKeys{"idle","ask_hug","shiver","walk"};
    for(const auto &key:animatedKeys) {
        QVector<QPixmap> frames;
        for(const auto &root:animationRoots) {
            frames.clear();
            for(int i=1;i<=12;++i) {
                const QString path=QString("%1/%2/frame_%3.png").arg(root,key).arg(i,2,10,QChar('0'));
                if(!QFileInfo::exists(path)) break;
                QPixmap frame;
                if(frame.load(path)) frames.push_back(frame);
            }
            if(!frames.isEmpty()) break;
        }
        if(!frames.isEmpty()) animationAssets_.insert(key,frames);
    }
}

QString PetWindow::assetKeyForAction(Action action) const {
    switch(action){
    case Action::Idle:return "idle";
    case Action::Bob:return "working";
    case Action::Walk:return "walk";
    case Action::Think:return "thinking";
    case Action::Celebrate:return "celebrate";
    case Action::Sleep:return "sleep";
    case Action::Shiver:return "shiver";
    case Action::AskHug:return "ask_hug";
    case Action::Hug:return "hug";
    case Action::Blush:return "blush";
    case Action::BlushWave:return "blush_wave";
    case Action::Study:return "study";
    case Action::AdjustGlasses:return "adjust_glasses";
    case Action::RemoveGlasses:return "remove_glasses";
    case Action::Wave:return "wave";
    }
    return "idle";
}

int PetWindow::frameStrideForAction(Action action) const {
    switch(action) {
    case Action::Idle:return 12;      // gentle blink/breathe cadence
    case Action::AskHug:return 6;    // paws extend slowly
    case Action::Shiver:return 2;    // quick cold tremble
    case Action::Walk:return 4;       // readable little steps
    default:return 5;
    }
}

const QPixmap *PetWindow::pixmapForAction(Action action) const {
    const QString key=assetKeyForAction(action);
    const auto ait=animationAssets_.constFind(key);
    if(ait!=animationAssets_.constEnd() && !ait.value().isEmpty()) {
        const int stride=qMax(1,frameStrideForAction(action));
        const int index=(frame_/stride)%ait.value().size();
        return &ait.value().at(index);
    }
    auto it=stateAssets_.constFind(key);
    if(it!=stateAssets_.constEnd()) return &it.value();
    it=stateAssets_.constFind("idle");
    if(it!=stateAssets_.constEnd()) return &it.value();
    return pet_.isNull() ? nullptr : &pet_;
}

void PetWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.setRenderHint(QPainter::SmoothPixmapTransform,true);

    const qreal t=frame_/8.0;
    int dx=0;
    int dy=0;
    qreal scale=1.0;
    qreal rotation=0.0;

    switch(action_) {
    case Action::Idle:
        dy=int(2*qSin(t));
        scale=1.0+0.006*qSin(t*.55);
        break;
    case Action::Bob:
        dy=int(5*qSin(t*1.5));
        scale=1.0+0.008*qSin(t*.65);
        break;
    case Action::Walk:
        dy=-qAbs(int(2*qSin(t*2.2)));
        rotation=1.2*qSin(t*2.2);
        break;
    case Action::Think:
        dy=int(2*qSin(t));
        rotation=-1.4+0.8*qSin(t*.7);
        scale=1.0+0.009*qSin(t*.8);
        break;
    case Action::Celebrate:
        dy=-qAbs(int(10*qSin(t*1.8)));
        scale=1.0+0.025*qSin(t*1.8);
        rotation=2.5*qSin(t*1.8);
        break;
    case Action::Sleep:
        dy=int(qSin(t*.35));
        scale=.99+0.008*qSin(t*.35);
        rotation=-1.5;
        break;
    case Action::Shiver:
        dx=(frame_%4<2)?-2:2;
        dy=int(qSin(t));
        scale=.995;
        break;
    case Action::AskHug:
        dy=-qAbs(int(3*qSin(t*1.2)));
        scale=1.01+0.018*qSin(t*.9);
        rotation=1.0*qSin(t*.7);
        break;
    case Action::Hug:
        scale=1.055+0.02*qSin(t*.8);
        dy=-3;
        rotation=1.5*qSin(t*.65);
        break;
    case Action::Blush:
        dy=int(2*qSin(t*.8));
        rotation=1.8*qSin(t*.55);
        scale=1.012;
        break;
    case Action::BlushWave:
        dy=-qAbs(int(3*qSin(t*1.2)));
        rotation=2.5*qSin(t*.9);
        scale=1.018;
        break;
    case Action::Study:
        dy=int(2*qSin(t*1.0));
        rotation=-.8;
        break;
    case Action::AdjustGlasses:
        dy=-qAbs(int(2*qSin(t*1.6)));
        rotation=-1.8*qSin(t*1.2);
        break;
    case Action::RemoveGlasses:
        scale=1.04+0.012*qSin(t*.8);
        rotation=-1.0;
        break;
    case Action::Wave:
        dy=-qAbs(int(3*qSin(t*1.6)));
        rotation=3.0*qSin(t*1.8);
        break;
    }

    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0,0,0,38));
    const qreal shadowScale=(action_==Action::Celebrate ? .78 : action_==Action::Walk ? .9 : 1.0);
    const qreal sw=112*shadowScale;
    p.drawEllipse(QRectF(width()/2.0-sw/2.0,196,sw,13));
    p.restore();

    const QPointF center(width()/2.0+dx,112+dy);
    const QSizeF size(200*scale,200*scale);
    QRectF target(QPointF(-size.width()/2.0,-size.height()/2.0),size);

    p.save();
    p.translate(center);
    p.rotate(rotation);
    if(const QPixmap *sprite=pixmapForAction(action_)) {
        p.drawPixmap(target.toRect(),*sprite);
    } else {
        p.setBrush(QColor(246,220,181));
        p.setPen(QPen(QColor(70,45,35),3));
        p.drawEllipse(QRectF(-75,-75,150,150));
        p.setPen(QColor(35,35,35));
        QFont f=p.font(); f.setBold(true); f.setPointSize(18); p.setFont(f);
        p.drawText(QRectF(-95,-30,190,60),Qt::AlignCenter,"TONY");
    }
    p.restore();

    const QRect namePlate(width()/2-35,218,70,23);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20,20,20,105));
    p.drawRoundedRect(namePlate,11,11);
    p.setPen(QColor(255,255,255,245));
    QFont f("Microsoft YaHei UI",10,QFont::DemiBold);
    p.setFont(f);
    p.drawText(namePlate,Qt::AlignCenter,"Tony");
}

void PetWindow::setAction(Action a,int durationMs){
    action_=a;
    frame_=0;
    basePos_=pos();
    if(durationMs>0) actionTimer_.start(durationMs); else actionTimer_.stop();
    update();
}

PetWindow::Action PetWindow::baseActionForAgentState() const {
    if(agentState_=="thinking") return Action::Think;
    if(agentState_=="working" || agentState_=="tool_running") return Action::Bob;
    if(agentState_=="sleeping") return Action::Sleep;
    if(agentState_=="error") return Action::Think;
    return Action::Idle;
}

void PetWindow::restoreAgentAction(){
    action_=baseActionForAgentState();
    frame_=0;
    basePos_=pos();
    update();
}

PetWindow::Action PetWindow::actionFromWire(const QString &name) const {
    const auto n=name.trimmed().toLower();
    if(n=="working") return Action::Bob;
    if(n=="walk" || n=="walking") return Action::Walk;
    if(n=="thinking" || n=="think" || n=="pose_tough" || n=="scratch_head" || n=="embarrassed") return Action::Think;
    if(n=="celebrate" || n=="happy_bounce" || n=="handsome_pose" || n=="smug_blink") return Action::Celebrate;
    if(n=="sleep") return Action::Sleep;
    if(n=="shiver") return Action::Shiver;
    if(n=="ask_hug" || n=="offer_hug") return Action::AskHug;
    if(n=="hug") return Action::Hug;
    if(n=="blush_wave") return Action::BlushWave;
    if(n=="blush" || n=="offer_scarf") return Action::Blush;
    if(n=="study" || n=="study_tired" || n=="study_cozy") return Action::Study;
    if(n=="adjust_glasses") return Action::AdjustGlasses;
    if(n=="remove_glasses") return Action::RemoveGlasses;
    if(n=="wave" || n=="paw_wave" || n=="ear_wiggle" || n=="wake") return Action::Wave;
    if(n=="blanket" || n=="warm_hands" || n=="tea") return Action::Shiver;
    if(n=="quiet_idle" || n=="soft_idle" || n=="calm_sit" || n=="proud_sit" || n=="sit" || n=="eat") return Action::Idle;
    return Action::Idle;
}

void PetWindow::tickAnimation(){
    ++frame_;
    if(action_==Action::Walk && !dragging_){
        auto screen=QGuiApplication::screenAt(frameGeometry().center());
        if(!screen) screen=QGuiApplication::primaryScreen();
        const auto area=screen->availableGeometry();
        QPoint n=pos()+QPoint(2*walkDirection_,0);
        if(n.x()+width()>area.right()) {
            walkDirection_=-1;
            n.setX(area.right()-width());
        } else if(n.x()<area.left()) {
            walkDirection_=1;
            n.setX(area.left());
        }
        move(n);
    }
    bubble_.follow(mapToGlobal(QPoint(width()/2,20)));
    update();
}

void PetWindow::scheduleIdleMoment(){
    idleTimer_.start(QRandomGenerator::global()->bounded(18000,46001));
}

void PetWindow::runIdleMoment(){
    if(action_!=Action::Idle || dragging_ || agentState_!="idle") {
        scheduleIdleMoment();
        return;
    }

    const int hour=QTime::currentTime().hour();
    const bool night=(hour>=23 || hour<7);
    const int r=QRandomGenerator::global()->bounded(100);

    if(night && r<22) {
        emotion_="sleepy";
        setAction(Action::Sleep,QRandomGenerator::global()->bounded(6500,11001));
    } else if(r<12) {
        emotion_="cold";
        setAction(Action::Shiver,2800);
        showBubble("Brrr... Tony wants somewhere warm.",4200);
    } else if(r<23) {
        emotion_="hopeful";
        setAction(Action::AskHug,3400);
        showBubble("Can I have a tiny hug?",4200);
    } else if(r<36) {
        emotion_="playful";
        setAction(Action::Walk,QRandomGenerator::global()->bounded(4200,7201));
    } else if(r<49) {
        emotion_="curious";
        setAction(Action::Think,2400);
    } else if(r<62) {
        emotion_="friendly";
        setAction(Action::Wave,1800);
    } else if(r<71) {
        emotion_="focused";
        setAction(Action::AdjustGlasses,2000);
    }

    scheduleIdleMoment();
}

QString PetWindow::actionName() const {
    switch(action_){
    case Action::Idle:return "idle";
    case Action::Bob:return "working";
    case Action::Walk:return "walking";
    case Action::Think:return "thinking";
    case Action::Celebrate:return "celebrating";
    case Action::Sleep:return "sleeping";
    case Action::Shiver:return "shivering";
    case Action::AskHug:return "asking for a hug";
    case Action::Hug:return "hugging";
    case Action::Blush:return "blushing";
    case Action::BlushWave:return "blushing for Paula";
    case Action::Study:return "studying";
    case Action::AdjustGlasses:return "adjusting glasses";
    case Action::RemoveGlasses:return "no-glasses mode";
    case Action::Wave:return "waving";
    }
    return "idle";
}

void PetWindow::enterEvent(QEnterEvent*){
    if(agentState_=="idle" && action_==Action::Idle && !actionTimer_.isActive() && !dragging_) {
        emotion_="friendly";
        setAction(Action::Wave,1200);
    }
}

void PetWindow::leaveEvent(QEvent*){}

void PetWindow::mousePressEvent(QMouseEvent *e){
    if(e->button()==Qt::LeftButton){
        dragging_=true;
        dragOffset_=e->globalPosition().toPoint()-frameGeometry().topLeft();
    }
}

void PetWindow::mouseMoveEvent(QMouseEvent *e){
    if(dragging_ && (e->buttons()&Qt::LeftButton)){
        move(e->globalPosition().toPoint()-dragOffset_);
        action_=Action::Bob;
        frame_=0;
        bubble_.follow(mapToGlobal(QPoint(width()/2,20)));
        update();
    }
}

void PetWindow::mouseReleaseEvent(QMouseEvent *e){
    if(e->button()==Qt::LeftButton){
        dragging_=false;
        savePosition();
        restoreAgentAction();
    }
}

void PetWindow::mouseDoubleClickEvent(QMouseEvent *e){
    if(e->button()==Qt::LeftButton) askTony();
}

void PetWindow::contextMenuEvent(QContextMenuEvent *e){
    QMenu m;
    auto ask=m.addAction("问 Tony…");
    auto hug=m.addAction("抱抱 Tony");
    m.addSeparator();
    auto pair=m.addAction("连接 / 配对新设备…");
    auto localSsh=m.addAction("使用本机 SSH 连接");
    m.addSeparator();
    auto idle=m.addAction("坐好");
    auto walk=m.addAction("散步");
    auto think=m.addAction("思考");
    auto study=m.addAction("学习化学");
    auto glasses=m.addAction("扶一下眼镜");
    auto noGlasses=m.addAction("摘掉眼镜耍帅");
    auto paula=m.addAction("Paula 来了");
    auto celebrate=m.addAction("开心一下");
    auto cold=m.addAction("有点冷");
    auto sleep=m.addAction("睡觉");
    m.addSeparator();
    auto quit=m.addAction("退出 Tony");

    auto chosen=m.exec(e->globalPos());
    if(chosen==ask) askTony();
    else if(chosen==hug) hugTony();
    else if(chosen==pair) configureConnection();
    else if(chosen==localSsh) useLocalSshConnection();
    else if(chosen==idle) setAction(Action::Idle);
    else if(chosen==walk) setAction(Action::Walk,6000);
    else if(chosen==think) setAction(Action::Think,4000);
    else if(chosen==study) setAction(Action::Study,5000);
    else if(chosen==glasses) setAction(Action::AdjustGlasses,2200);
    else if(chosen==noGlasses) setAction(Action::RemoveGlasses,3500);
    else if(chosen==paula) {
        emotion_="bashful";
        setAction(Action::BlushWave,3600);
        showBubble("Paula? Wait—do I look okay?",4200);
    }
    else if(chosen==celebrate) setAction(Action::Celebrate,2200);
    else if(chosen==cold) {
        emotion_="cold";
        setAction(Action::Shiver,3000);
        showBubble("Brrr... warm paws, please.",4200);
    }
    else if(chosen==sleep) setAction(Action::Sleep);
    else if(chosen==quit) qApp->quit();
}

void PetWindow::askTony(){
    bool ok=false;
    auto text=QInputDialog::getText(this,"Tony","想让我做什么？",QLineEdit::Normal,{},&ok);
    if(!ok||text.trimmed().isEmpty()) return;
    answer_.clear();
    emotion_="curious";
    agentState_="thinking";
    restoreAgentAction();
    if(agent_.connected()) agent_.sendMessage(text);
    else showBubble("服务器还没连接好。右键 Tony 可以选择“连接 / 配对新设备…”。\n\n"+text,6500);
}

void PetWindow::hugTony(){
    emotion_="happy";
    setAction(Action::Hug,3000);
    showBubble("抱到啦。Tony 开心地蹭了蹭。",4300);
}

void PetWindow::configureConnection(){
    QSettings s;
    bool ok=false;
    const QString current=s.value("agent/url","wss://tony.example.com/agent/ws").toUrl().toString();
    const QString endpointText=QInputDialog::getText(
        this,"连接 Tony","服务器地址（WSS）：",QLineEdit::Normal,current,&ok);
    if(!ok) return;

    const QUrl endpoint(endpointText.trimmed());
    const auto scheme=endpoint.scheme().toLower();
    if(!endpoint.isValid() || endpoint.host().isEmpty() || (scheme!="ws" && scheme!="wss")) {
        QMessageBox::warning(this,"Tony","地址格式不正确。示例：wss://example.com/agent/ws");
        return;
    }

    const QString code=QInputDialog::getText(
        this,"配对 Tony","输入服务器生成的一次性配对码：",QLineEdit::Normal,{},&ok);
    if(!ok || code.trimmed().isEmpty()) return;

    const QString defaultName=QSysInfo::machineHostName().isEmpty() ? QString("Tony desktop") : QSysInfo::machineHostName();
    const QString deviceName=QInputDialog::getText(
        this,"设备名称","给这台电脑起个名字：",QLineEdit::Normal,defaultName,&ok);
    if(!ok) return;

    emotion_="curious";
    setAction(Action::Think,0);
    tray_.setToolTip("Tony · pairing…");
    showBubble("正在安全配对这台电脑…",0);
    agent_.pairAndConnect(endpoint,code,deviceName);
}

void PetWindow::useLocalSshConnection(){
    QSettings s;
    const QUrl endpoint("ws://127.0.0.1:18790/agent/ws");
    s.setValue("agent/url",endpoint);
    s.remove("agent/token");
    s.remove("agent/device_id");
    tunnel_.start();
    agent_.connectTo(endpoint,{});
    emotion_="friendly";
    showBubble("已切回本机 SSH 隧道模式。Tony 不会在程序里保存 SSH 私钥。",5600);
}

void PetWindow::showBubble(const QString &text, int timeoutMs){
    bubble_.showMessage(text,mapToGlobal(QPoint(width()/2,20)),emotion_,timeoutMs);
}

void PetWindow::restorePosition(){
    QSettings s;
    auto v=s.value("pet/position");
    if(v.isValid()) move(v.toPoint());
    else {
        auto a=QGuiApplication::primaryScreen()->availableGeometry();
        move(a.right()-width()-40,a.bottom()-height()-20);
    }
}

void PetWindow::savePosition(){
    QSettings().setValue("pet/position",pos());
}
