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

QString uiLanguage() {
    const QString n=QSettings().value("ui/language","en").toString().trimmed().toLower();
    return (n.startsWith("zh") || n=="cn") ? QStringLiteral("zh") : QStringLiteral("en");
}

QString uiText(const QString &en, const QString &zh) {
    return uiLanguage()=="zh" ? zh : en;
}

QString defaultPublicEndpoint() {
    return QStringLiteral("wss://150-158-27-206.sslip.io/agent/ws");
}
}

PetWindow::PetWindow(QWidget *parent)
    : QWidget(parent), localBridge_(this), bubble_(nullptr), composer_(nullptr) {
    setWindowTitle("Tony");
    setFixedSize(230,250);
    setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint|Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    loadAssets();
    restorePosition();

    QSettings initialSettings;
    if(!initialSettings.contains("ui/language")) initialSettings.setValue("ui/language","en");
    agent_.setLanguage(uiLanguage());
    composer_.setLanguage(uiLanguage());

    // Calm desktop-companion cadence: animation is deliberately low-frame-rate.
    // At normal idle Tony stays still; only an occasional blink/wink changes the sprite.
    animTimer_.setInterval(110);
    connect(&animTimer_, &QTimer::timeout, this, &PetWindow::tickAnimation);
    animTimer_.start();

    blinkTimer_.setSingleShot(true);
    connect(&blinkTimer_, &QTimer::timeout, this, [this]{
        if(action_==Action::Idle && agentState_=="idle" && !dragging_ && !composer_.isVisible()) {
            idleBlinking_=true;
            idleBlinkTick_=0;
            update();
        } else {
            scheduleBlink();
        }
    });
    scheduleBlink();

    idleTimer_.setSingleShot(true);
    connect(&idleTimer_, &QTimer::timeout, this, &PetWindow::runIdleMoment);
    scheduleIdleMoment();

    actionTimer_.setSingleShot(true);
    connect(&actionTimer_, &QTimer::timeout, this, &PetWindow::restoreAgentAction);

    connect(&composer_,&ChatComposer::submitted,this,[this](const QString &text){
        submitTonyPrompt(text);
    });

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
        if(!actionTimer_.isActive()) restoreAgentAction();
    });
    connect(&agent_, &AgentClient::toolRequest, this,
            [this](const QString &requestId, const QString &tool, const QJsonObject &args){
        agentState_="tool_running";
        emotion_="focused";
        restoreAgentAction();
        tray_.setToolTip(QString("Tony · local tool · %1").arg(tool));
        localBridge_.execute(requestId,tool,args);
    });
    connect(&localBridge_, &LocalBridge::finished, this,
            [this](const QString &requestId, const QString &tool, bool ok,
                   const QJsonObject &result, const QString &error){
        agent_.sendToolResult(requestId,tool,ok,result,error);
        if(!ok) {
            emotion_="gentle";
            showBubble("本地操作没有执行："+error.left(220),4500);
        }
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
        setAction(Action::Celebrate,2100);
        tray_.setToolTip("Tony · paired · connecting");
        showBubble(uiText("Paired. Tony will remember this computer.","配对成功。Tony 会记住这台电脑。"),5200);
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
            QSettings cs;
            const auto active=cs.value("agent/url",defaultPublicEndpoint()).toUrl();
            if(active.host()=="127.0.0.1" || active.host()=="localhost")
                showBubble(uiText("Local SSH tunnel is not ready. Open Settings → Connection to return to the public server.","本机 SSH 隧道尚未就绪。可在设置 → 连接中切回公网服务器。"),6500);
        }
    });

    tray_.setToolTip("Tony · Desktop Agent");
    tray_.setVisible(true);
    localBridge_.setTrayIcon(&tray_);
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r){
        if(r==QSystemTrayIcon::Trigger){ show(); raise(); }
    });

    QSettings s;
    QUrl endpoint=s.value("agent/url",defaultPublicEndpoint()).toUrl();
    const auto token=unprotectSecret(s.value("agent/token","").toString());
    const bool preferLocal=s.value("connection/prefer_local_ssh",false).toBool();
    if((endpoint.host()=="127.0.0.1" || endpoint.host()=="localhost") && !preferLocal) {
        endpoint=QUrl(s.value("agent/public_url",defaultPublicEndpoint()).toString());
        s.setValue("agent/url",endpoint);
    }
    if(!token.isEmpty()) {
        if(endpoint.host()=="127.0.0.1" || endpoint.host()=="localhost") tunnel_.start();
        agent_.connectTo(endpoint,token);
    } else {
        tray_.setToolTip("Tony · not paired");
        showBubble(uiText("Hi Paula. Right-click me and choose Connect to Tony.","嗨 Paula。右键点我，然后选择“连接 Tony”。"),6500);
    }
}

PetWindow::~PetWindow() {
    composer_.dismiss();
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

    const QStringList keys{
        "idle","working","walk","thinking","celebrate","sleep","shiver",
        "ask_hug","hug","blush","blush_wave","study","adjust_glasses","remove_glasses","wave"
    };

    const QStringList stateRoots{
        appDir+"/assets/states",
        QDir::currentPath()+"/assets/states"
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
    for(const auto &key:keys) {
        for(const auto &root:animationRoots) {
            QDir dir(root+"/"+key);
            if(!dir.exists()) continue;
            const QStringList files=dir.entryList(QStringList{"frame_*.png"},QDir::Files,QDir::Name);
            QVector<QPixmap> frames;
            frames.reserve(files.size());
            for(const auto &file:files) {
                QPixmap frame;
                if(frame.load(dir.filePath(file))) frames.push_back(frame);
            }
            if(frames.size()>=2) {
                animationAssets_.insert(key,frames);
                break;
            }
        }
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
    case Action::Shiver:return 2;
    case Action::Walk:return 3;
    case Action::Celebrate:return 3;
    case Action::BlushWave:return 4;
    case Action::Wave:return 4;
    case Action::AskHug:return 5;
    case Action::Hug:return 5;
    case Action::Study:return 5;
    case Action::AdjustGlasses:return 5;
    case Action::RemoveGlasses:return 6;
    case Action::Think:return 5;
    case Action::Bob:return 4;
    case Action::Sleep:return 8;
    case Action::Blush:return 6;
    case Action::Idle:return 7;
    }
    return 5;
}

const QPixmap *PetWindow::pixmapForAction(Action action) const {
    const QString key=assetKeyForAction(action);

    // Visual-quality rollback: the old multi-frame PNG sequences were authored
    // with inconsistent crops and character proportions. They made Tony appear
    // to lose feet/ears/arms between frames. Until a single approved master
    // character sheet is available, use the higher-quality complete state art
    // and let paintEvent provide the motion. This keeps Tony visually stable.
    auto it=stateAssets_.constFind(key);
    if(it!=stateAssets_.constEnd() && !it.value().isNull()) return &it.value();

    it=stateAssets_.constFind("idle");
    if(it!=stateAssets_.constEnd() && !it.value().isNull()) return &it.value();

    return pet_.isNull() ? nullptr : &pet_;
}

void PetWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.setRenderHint(QPainter::SmoothPixmapTransform,true);

    const qreal t=frame_/8.0;
    int dx=0, dy=0;
    qreal scale=1.0, rotation=0.0;
    switch(action_) {
    case Action::Idle:
        // Normal desktop state is intentionally motionless. The face sprite
        // handles the occasional blink; no perpetual bobbing or breathing zoom.
        break;
    case Action::Bob: dy=int(2*qSin(t*.65)); scale=1.0+0.003*qSin(t*.45); break;
    case Action::Walk: dy=-qAbs(int(qSin(t*1.25))); rotation=.7*qSin(t*1.25); break;
    case Action::Think: rotation=-.7+.35*qSin(t*.45); scale=1.0+0.003*qSin(t*.4); break;
    case Action::Celebrate: dy=-qAbs(int(6*qSin(t*1.05))); scale=1.0+0.015*qSin(t*1.05); rotation=1.4*qSin(t*1.05); break;
    case Action::Sleep: dy=int(qSin(t*.35)); scale=.99+0.008*qSin(t*.35); rotation=-1.5; break;
    case Action::Shiver: dx=(frame_%4<2)?-2:2; dy=int(qSin(t)); scale=.995; break;
    case Action::AskHug: dy=-qAbs(int(3*qSin(t*1.2))); scale=1.01+0.018*qSin(t*.9); rotation=1.0*qSin(t*.7); break;
    case Action::Hug: scale=1.055+0.02*qSin(t*.8); dy=-3; rotation=1.5*qSin(t*.65); break;
    case Action::Blush: dy=int(2*qSin(t*.8)); rotation=1.8*qSin(t*.55); scale=1.012; break;
    case Action::BlushWave: dy=-qAbs(int(3*qSin(t*1.2))); rotation=2.5*qSin(t*.9); scale=1.018; break;
    case Action::Study: dy=int(2*qSin(t*1.0)); rotation=-.8; break;
    case Action::AdjustGlasses: dy=-qAbs(int(2*qSin(t*1.6))); rotation=-1.8*qSin(t*1.2); break;
    case Action::RemoveGlasses: scale=1.04+0.012*qSin(t*.8); rotation=-1.0; break;
    case Action::Wave: dy=-qAbs(int(3*qSin(t*1.6))); rotation=3.0*qSin(t*1.8); break;
    }

    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0,0,0,38));
    const qreal shadowScale=(action_==Action::Celebrate ? .78 : action_==Action::Walk ? .9 : 1.0);
    const qreal sw=112*shadowScale;
    p.drawEllipse(QRectF(width()/2.0-sw/2.0,196,sw,13));
    p.restore();

    p.save();
    if(const QPixmap *sprite=pixmapForAction(action_); sprite && !sprite->isNull()) {
        // Fit the visible (alpha) subject, not the square PNG canvas. Different
        // source files can have different transparent padding; drawing every
        // canvas into a hard-coded box made the character jump in size.
        const QImage image=sprite->toImage().convertToFormat(QImage::Format_ARGB32);
        int minX=image.width(), minY=image.height(), maxX=-1, maxY=-1;
        for(int y=0; y<image.height(); ++y) {
            const QRgb *line=reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for(int x=0; x<image.width(); ++x) {
                if(qAlpha(line[x])>8) {
                    minX=qMin(minX,x); minY=qMin(minY,y);
                    maxX=qMax(maxX,x); maxY=qMax(maxY,y);
                }
            }
        }

        QRect source(0,0,image.width(),image.height());
        if(maxX>=minX && maxY>=minY) {
            source=QRect(QPoint(minX,minY),QPoint(maxX,maxY))
                       .adjusted(-6,-6,6,6)
                       .intersected(QRect(0,0,image.width(),image.height()));
        }

        const qreal maxMotionScale=(action_==Action::Hug) ? 1.04 : 1.025;
        const qreal safeMotionScale=qBound<qreal>(0.965,scale,maxMotionScale);
        const qreal maxW=184.0*safeMotionScale;
        const qreal maxH=194.0*safeMotionScale;
        const qreal fit=qMin(maxW/qMax(1,source.width()),
                             maxH/qMax(1,source.height()));
        const QSizeF drawSize(source.width()*fit,source.height()*fit);

        // All poses share one desktop ground line. Wide/short poses such as
        // sleep, study and working used to be vertically centered and appeared
        // to float 10-22 px above their shadow.
        const qreal groundY=205.0+dy;
        const QPointF spriteCenter(width()/2.0+dx,
                                   groundY-drawSize.height()/2.0);
        p.translate(spriteCenter);
        p.rotate(rotation);
        if(action_==Action::Walk && walkDirection_<0) p.scale(-1.0,1.0);

        const QRectF target(-drawSize.width()/2.0,-drawSize.height()/2.0,
                            drawSize.width(),drawSize.height());
        p.drawPixmap(target,*sprite,QRectF(source));
    } else {
        p.translate(QPointF(width()/2.0+dx,108+dy));
        p.rotate(rotation);
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
    action_=a; frame_=0; basePos_=pos();
    if(a!=Action::Idle) {
        idleBlinking_=false;
        idleBlinkTick_=0;
    } else if(!blinkTimer_.isActive()) {
        scheduleBlink();
    }
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
    action_=baseActionForAgentState(); frame_=0; basePos_=pos();
    if(action_==Action::Idle && !blinkTimer_.isActive()) scheduleBlink();
    if(action_!=Action::Idle) { idleBlinking_=false; idleBlinkTick_=0; }
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
    if(n=="blush_wave" || n=="paula_blush" || n=="paula_special") return Action::BlushWave;
    if(n=="blush" || n=="offer_scarf") return Action::Blush;
    if(n=="study" || n=="study_tired" || n=="study_cozy") return Action::Study;
    if(n=="adjust_glasses") return Action::AdjustGlasses;
    if(n=="remove_glasses") return Action::RemoveGlasses;
    if(n=="wave" || n=="paw_wave" || n=="ear_wiggle" || n=="wake") return Action::Wave;
    if(n=="blanket" || n=="warm_hands" || n=="tea") return Action::Shiver;
    return Action::Idle;
}

void PetWindow::tickAnimation(){
    ++frame_;
    if(action_==Action::Idle && idleBlinking_) {
        ++idleBlinkTick_;
        if(idleBlinkTick_>=7) {
            idleBlinking_=false;
            idleBlinkTick_=0;
            scheduleBlink();
        }
    }
    if(action_==Action::Walk && !dragging_){
        auto screen=QGuiApplication::screenAt(frameGeometry().center());
        if(!screen) screen=QGuiApplication::primaryScreen();
        const auto area=screen->availableGeometry();
        // Slow desktop walk: advance only on every second animation tick.
        // This preserves the original native-motion feel without making Tony pace.
        QPoint n=pos()+QPoint((frame_%2==0 ? 1 : 0)*walkDirection_,0);
        if(n.x()+width()>area.right()) { walkDirection_=-1; n.setX(area.right()-width()); }
        else if(n.x()<area.left()) { walkDirection_=1; n.setX(area.left()); }
        move(n);
    }
    const QPoint anchor=mapToGlobal(QPoint(width()/2,20));
    bubble_.follow(anchor);
    composer_.follow(anchor);
    if(action_!=Action::Idle || idleBlinking_) update();
}

void PetWindow::scheduleBlink(){
    if(blinkTimer_.isActive()) return;
    // Natural but quiet: keep long pauses so Tony does not look restless.
    blinkTimer_.start(QRandomGenerator::global()->bounded(7500,15001));
}

void PetWindow::scheduleIdleMoment(){
    // Autonomous gestures stay rare. Tony should spend most of his time simply sitting.
    idleTimer_.start(QRandomGenerator::global()->bounded(140000,320001));
}

void PetWindow::runIdleMoment(){
    if(action_!=Action::Idle || dragging_ || agentState_!="idle" || composer_.isVisible()) {
        scheduleIdleMoment(); return;
    }
    const int hour=QTime::currentTime().hour();
    const bool night=(hour>=23 || hour<7);
    const int r=QRandomGenerator::global()->bounded(100);
    // Most checks intentionally do nothing. Tony should feel present, not restless.
    if(night && r<10) { emotion_="sleepy"; setAction(Action::Sleep,QRandomGenerator::global()->bounded(6500,9501)); }
    else if(r<4) { emotion_="cold"; setAction(Action::Shiver,3000); showBubble("Brrr... stay warm with me?",3600); }
    else if(r<8) { emotion_="hopeful"; setAction(Action::AskHug,3400); showBubble("Can I have a tiny hug?",3600); }
    else if(r<12) { emotion_="friendly"; setAction(Action::Wave,1900); }
    else if(r<15) { emotion_="focused"; setAction(Action::AdjustGlasses,2300); }
    else if(r<17) { emotion_="playful"; setAction(Action::Walk,4800); }
    scheduleIdleMoment();
}

QString PetWindow::actionName() const {
    switch(action_){
    case Action::Idle:return "idle"; case Action::Bob:return "working"; case Action::Walk:return "walking";
    case Action::Think:return "thinking"; case Action::Celebrate:return "celebrating"; case Action::Sleep:return "sleeping";
    case Action::Shiver:return "shivering"; case Action::AskHug:return "asking for a hug"; case Action::Hug:return "hugging";
    case Action::Blush:return "blushing"; case Action::BlushWave:return "blushing for Paula"; case Action::Study:return "studying";
    case Action::AdjustGlasses:return "adjusting glasses"; case Action::RemoveGlasses:return "no-glasses mode"; case Action::Wave:return "waving";
    }
    return "idle";
}

void PetWindow::enterEvent(QEnterEvent*){
    // Hovering should not make Tony constantly wave. Stay calm and let the blink timer work.
    if(agentState_=="idle" && action_==Action::Idle && !blinkTimer_.isActive()) scheduleBlink();
}
void PetWindow::leaveEvent(QEvent*){}

void PetWindow::mousePressEvent(QMouseEvent *e){
    if(e->button()==Qt::LeftButton){ dragging_=true; dragOffset_=e->globalPosition().toPoint()-frameGeometry().topLeft(); }
}
void PetWindow::mouseMoveEvent(QMouseEvent *e){
    if(dragging_ && (e->buttons()&Qt::LeftButton)){
        move(e->globalPosition().toPoint()-dragOffset_);
        action_=Action::Bob; frame_=0;
        const QPoint anchor=mapToGlobal(QPoint(width()/2,20));
        bubble_.follow(anchor); composer_.follow(anchor); update();
    }
}
void PetWindow::mouseReleaseEvent(QMouseEvent *e){
    if(e->button()==Qt::LeftButton){ dragging_=false; savePosition(); restoreAgentAction(); }
}
void PetWindow::mouseDoubleClickEvent(QMouseEvent *e){ if(e->button()==Qt::LeftButton) askTony(); }

void PetWindow::contextMenuEvent(QContextMenuEvent *e){
    QMenu m;
    auto ask=m.addAction(uiText("Chat with Tony…","和 Tony 聊天…"));
    auto hug=m.addAction(uiText("Hug Tony","抱抱 Tony"));
    auto paula=m.addAction(uiText("Paula is here","Paula 来了"));
    m.addSeparator();
    auto pair=m.addAction(agent_.connected() ? uiText("Reconnect / pair another computer…","重新连接 / 配对其他电脑…") : uiText("Connect to Tony…","连接 Tony…"));

    auto *settings=m.addMenu(uiText("Settings","设置"));
    auto *languageMenu=settings->addMenu(uiText("Language","语言"));
    auto english=languageMenu->addAction("English");
    auto chinese=languageMenu->addAction("简体中文");
    english->setCheckable(true); chinese->setCheckable(true);
    english->setChecked(uiLanguage()=="en"); chinese->setChecked(uiLanguage()=="zh");

    auto *connectionMenu=settings->addMenu(uiText("Connection","连接"));
    auto serverAddress=connectionMenu->addAction(uiText("Server address…","服务器地址…"));
    auto localSsh=connectionMenu->addAction(uiText("Use local SSH tunnel (advanced)","使用本机 SSH 隧道（高级）"));

    auto *actions=m.addMenu(uiText("Tony actions","Tony 动作"));
    auto idle=actions->addAction(uiText("Sit quietly","安静坐好"));
    auto walk=actions->addAction(uiText("Take a short walk","散一小会儿步"));
    auto glasses=actions->addAction(uiText("Adjust glasses","扶一下眼镜"));
    auto noGlasses=actions->addAction(uiText("Take off glasses","摘掉眼镜"));
    auto cold=actions->addAction(uiText("Feeling cold","有点冷"));
    auto sleep=actions->addAction(uiText("Sleep","睡觉"));

    m.addSeparator();
    auto quit=m.addAction(uiText("Quit Tony","退出 Tony"));

    auto chosen=m.exec(e->globalPos());
    if(chosen==ask) askTony();
    else if(chosen==hug) hugTony();
    else if(chosen==paula) { emotion_="bashful"; setAction(Action::BlushWave,2600); showBubble(uiText("Paula? Wait—do I look okay?","Paula？等等——我看起来还好吗？"),4200); }
    else if(chosen==pair) configureConnection();
    else if(chosen==english || chosen==chinese) {
        const QString lang=(chosen==chinese) ? "zh" : "en";
        QSettings().setValue("ui/language",lang);
        agent_.setLanguage(lang);
        composer_.setLanguage(lang);
        showBubble(lang=="zh" ? "语言已切换为简体中文。" : "Language changed to English.",3200);
    }
    else if(chosen==serverAddress) {
        QSettings s; bool ok=false;
        const QString current=s.value("agent/public_url",defaultPublicEndpoint()).toString();
        const QString value=QInputDialog::getText(this,uiText("Tony server","Tony 服务器"),uiText("WSS server address:","WSS 服务器地址："),QLineEdit::Normal,current,&ok).trimmed();
        if(ok && !value.isEmpty()) {
            const QUrl u(value);
            if(u.isValid() && u.scheme().toLower()=="wss" && !u.host().isEmpty()) {
                s.setValue("agent/public_url",u.toString());
                s.setValue("agent/url",u);
                s.setValue("connection/prefer_local_ssh",false);
                const QString token=unprotectSecret(s.value("agent/token","").toString());
                if(!token.isEmpty()) agent_.connectTo(u,token);
                showBubble(uiText("Server address saved.","服务器地址已保存。"),3200);
            } else QMessageBox::warning(this,"Tony",uiText("Please enter a valid wss:// address.","请输入有效的 wss:// 地址。"));
        }
    }
    else if(chosen==localSsh) useLocalSshConnection();
    else if(chosen==idle) setAction(Action::Idle);
    else if(chosen==walk) setAction(Action::Walk,4800);
    else if(chosen==glasses) setAction(Action::AdjustGlasses,2300);
    else if(chosen==noGlasses) setAction(Action::RemoveGlasses,3400);
    else if(chosen==cold) { emotion_="cold"; setAction(Action::Shiver,2200); showBubble(uiText("Brrr… warm paws, please.","好冷……给我暖暖爪子。"),3600); }
    else if(chosen==sleep) setAction(Action::Sleep);
    else if(chosen==quit) qApp->quit();
}

void PetWindow::askTony(){
    bubble_.dismiss(); emotion_="curious";
    if(agentState_=="idle") setAction(Action::Think,0);
    composer_.openAt(mapToGlobal(QPoint(width()/2,40)));
}

void PetWindow::submitTonyPrompt(const QString &text){
    const QString prompt=text.trimmed();
    if(prompt.isEmpty()) return;
    answer_.clear(); emotion_="curious"; agentState_="thinking"; restoreAgentAction();
    if(agent_.connected()) agent_.sendMessage(prompt);
    else {
        agentState_="idle"; setAction(Action::Think,2800);
        showBubble(uiText("Tony is not connected yet. Right-click me and choose Connect to Tony.\n\n", "Tony 还没有连接。右键点我并选择“连接 Tony”。\n\n")+prompt,6500);
    }
}

void PetWindow::hugTony(){ emotion_="happy"; setAction(Action::Hug,3400); showBubble(uiText("Got you. Tony cuddles closer.","抱到啦。Tony 开心地靠近了一点。"),4300); }

void PetWindow::configureConnection(){
    QSettings s; bool ok=false;
    const QUrl endpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
    if(!endpoint.isValid() || endpoint.scheme().toLower()!="wss" || endpoint.host().isEmpty()) {
        QMessageBox::warning(this,"Tony",uiText("The server address in Settings is invalid.","设置中的服务器地址无效。"));
        return;
    }
    const QString code=QInputDialog::getText(this,uiText("Connect to Tony","连接 Tony"),uiText("Friend code:","好友码："),QLineEdit::Normal,{},&ok);
    if(!ok || code.trimmed().isEmpty()) return;
    const QString deviceName=QSysInfo::machineHostName().isEmpty() ? QString("Tony desktop") : QSysInfo::machineHostName();
    s.setValue("connection/prefer_local_ssh",false);
    emotion_="curious"; setAction(Action::Think,0); tray_.setToolTip("Tony · pairing…");
    showBubble(uiText("Connecting securely…","正在安全连接…"),0);
    agent_.pairAndConnect(endpoint,code,deviceName);
}

void PetWindow::useLocalSshConnection(){
    QSettings s; const QUrl endpoint("ws://127.0.0.1:18790/agent/ws");
    s.setValue("agent/url",endpoint);
    s.setValue("connection/prefer_local_ssh",true);
    const QString token=unprotectSecret(s.value("agent/token","").toString());
    if(token.isEmpty()) {
        showBubble(uiText("Pair this computer first, then local SSH can reuse the same device token.","请先配对这台电脑，本机 SSH 会复用同一个设备令牌。"),5200);
        return;
    }
    tunnel_.start(); agent_.connectTo(endpoint,token); emotion_="friendly";
    showBubble(uiText("Using the local SSH tunnel. Keep the tunnel available while Tony is running.","已切换到本机 SSH 隧道。使用 Tony 时请保持隧道可用。"),5200);
}

void PetWindow::showBubble(const QString &text, int timeoutMs){ bubble_.showMessage(text,mapToGlobal(QPoint(width()/2,20)),emotion_,timeoutMs); }

void PetWindow::restorePosition(){
    QSettings s; auto v=s.value("pet/position");
    if(v.isValid()) move(v.toPoint());
    else { auto a=QGuiApplication::primaryScreen()->availableGeometry(); move(a.right()-width()-40,a.bottom()-height()-20); }
}
void PetWindow::savePosition(){ QSettings().setValue("pet/position",pos()); }
