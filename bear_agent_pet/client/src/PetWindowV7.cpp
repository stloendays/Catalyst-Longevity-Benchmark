#include "PetWindow.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
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
#include <limits>

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
    behavior_.restore(initialSettings);
    dockMode_=dockModeFromName(initialSettings.value("pet/dock_mode","free").toString());
    dockScreenName_=initialSettings.value("pet/dock_screen","").toString();
    followCursor_=initialSettings.value("desktop/follow_cursor",true).toBool();
    snapToEdges_=initialSettings.value("desktop/snap_to_edges",true).toBool();
    hideForFullscreen_=initialSettings.value("desktop/hide_for_fullscreen",true).toBool();
    perchOnActiveWindow_=initialSettings.value("desktop/perch_on_active_window",false).toBool();
    gravityEnabled_=initialSettings.value("desktop/gravity",true).toBool();
    fastCursorChase_=initialSettings.value("desktop/fast_cursor_chase",true).toBool();
    autoRest_=initialSettings.value("desktop/auto_rest",true).toBool();
    edgePeek_=initialSettings.value("desktop/edge_peek",true).toBool();
    lastCursorGlobal_=QCursor::pos();
    ensureOnDesktop();
    lifeClock_.start();
    activityClock_.start();
    pressClock_.invalidate();
    rapidClickClock_.invalidate();
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

    hoverTimer_.setSingleShot(true);
    connect(&hoverTimer_, &QTimer::timeout, this, [this]{
        if(hovered_ && !dragging_ && action_==Action::Idle && agentState_=="idle" && !composer_.isVisible()) {
            emotion_="curious";
            setAction(Action::Curious,1100);
        }
    });

    lifeTimer_.setInterval(5000);
    connect(&lifeTimer_, &QTimer::timeout, this, &PetWindow::tickLife);
    lifeTimer_.start();

    // Desktop sensing runs at 4 Hz: quick enough to notice cursor sweeps without
    // turning foreground-window checks into a busy loop.
    desktopTimer_.setInterval(250);
    connect(&desktopTimer_, &QTimer::timeout, this, &PetWindow::tickDesktop);
    desktopTimer_.start();

    physicsTimer_.setInterval(16);
    connect(&physicsTimer_, &QTimer::timeout, this, &PetWindow::tickPhysics);

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
            setAction(Action::Wave,900);
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
        setAction(Action::Celebrate,1400);
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
    QSettings lifeSettings;
    behavior_.save(lifeSettings);
    savePosition();
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
        "idle","curious","pet","carried","land","dizzy","stretch","yawn",
        "working","walk","thinking","celebrate","sleep","shiver",
        "ask_hug","hug","blush","blush_wave","study","adjust_glasses","remove_glasses","wave",
        "pat","lifted","landing","cursor_watch"
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
    case Action::Curious:return "curious";
    case Action::Peek:return "idle";
    case Action::Pet:return "pet";
    case Action::Carried:return "carried";
    case Action::Land:return "land";
    case Action::Dizzy:return "dizzy";
    case Action::Stretch:return "stretch";
    case Action::Yawn:return "yawn";
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
    case Action::Carried:return 3;
    case Action::Land:return 3;
    case Action::Dizzy:return 2;
    case Action::Curious:return 5;
    case Action::Peek:return 5;
    case Action::Pet:return 4;
    case Action::Stretch:return 5;
    case Action::Yawn:return 7;
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

    // Idle may use only the short face-blink sequence. Do not loop the legacy
    // full-body animation sets: their crops/proportions vary and caused Tony to
    // appear to lose ears, feet or arms between frames.
    if(action==Action::Idle) {
        auto framesIt=animationAssets_.constFind("idle");
        if(idleBlinking_ && framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty()) {
            static constexpr int blinkSequence[] = {0, 1, 2, 2, 1, 0, 0};
            const int sequenceSize=static_cast<int>(sizeof(blinkSequence)/sizeof(blinkSequence[0]));
            const int step=qBound(0,idleBlinkTick_,sequenceSize-1);
            const int index=blinkSequence[step] % framesIt.value().size();
            return &framesIt.value().at(index);
        }
        auto idleIt=stateAssets_.constFind("idle");
        if(idleIt!=stateAssets_.constEnd() && !idleIt.value().isNull()) return &idleIt.value();
    }

    // Prefer the complete V0.9 state image. V0.8-only behaviors can consume the
    // newer approved interaction sequences when those assets are present.
    auto stateIt=stateAssets_.constFind(key);
    if(stateIt!=stateAssets_.constEnd() && !stateIt.value().isNull()) return &stateIt.value();

    QString authoredAlias;
    switch(action) {
    case Action::Curious: authoredAlias="cursor_watch"; break;
    case Action::Pet: authoredAlias="pat"; break;
    case Action::Carried: authoredAlias="lifted"; break;
    case Action::Land: authoredAlias="landing"; break;
    case Action::Dizzy: authoredAlias="dizzy"; break;
    case Action::Stretch: authoredAlias="stretch"; break;
    case Action::Yawn: authoredAlias="yawn"; break;
    default: break;
    }
    if(!authoredAlias.isEmpty()) {
        auto framesIt=animationAssets_.constFind(authoredAlias);
        if(framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty()) {
            const int stride=qMax(1,frameStrideForAction(action));
            const int index=(frame_/stride)%framesIt.value().size();
            return &framesIt.value().at(index);
        }
        auto aliasState=stateAssets_.constFind(authoredAlias);
        if(aliasState!=stateAssets_.constEnd() && !aliasState.value().isNull()) return &aliasState.value();
    }

    // Semantic fallbacks keep the same approved Tony design even before a
    // dedicated asset exists for every restored behavior.
    QString fallback="idle";
    if(action==Action::Pet) fallback="blush";
    else if(action==Action::Dizzy) fallback="thinking";
    else if(action==Action::Yawn) fallback="sleep";
    auto fallbackIt=stateAssets_.constFind(fallback);
    if(fallbackIt!=stateAssets_.constEnd() && !fallbackIt.value().isNull()) return &fallbackIt.value();

    auto idleIt=stateAssets_.constFind("idle");
    if(idleIt!=stateAssets_.constEnd() && !idleIt.value().isNull()) return &idleIt.value();
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
    case Action::Curious: dy=-1; rotation=1.4*qSin(t*.45); scale=1.008; break;
    case Action::Peek:
        if(dockMode_==DockMode::Left) { dx=-28; rotation=-4.0; }
        else if(dockMode_==DockMode::Right) { dx=28; rotation=4.0; }
        else if(dockMode_==DockMode::Top) { dy=-24; rotation=2.0*qSin(t*.55); }
        else { dy=-2; scale=1.01; }
        break;
    case Action::Pet: dy=2; scale=1.018+0.006*qSin(t*.75); rotation=1.0*qSin(t*.5); break;
    case Action::Carried: dy=-7; scale=.985; rotation=2.2*qSin(t*.9); break;
    case Action::Land: dy=-qAbs(int(5*qSin(t*1.55))); scale=1.0+0.012*qSin(t*1.55); break;
    case Action::Dizzy: dx=int(3*qSin(t*2.2)); rotation=4.5*qSin(t*1.7); scale=.985; break;
    case Action::Stretch: dy=-qAbs(int(3*qSin(t*.8))); scale=1.02+0.012*qSin(t*.65); break;
    case Action::Yawn: dy=2; rotation=-2.0; scale=.992+0.006*qSin(t*.35); break;
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

    // The desktop surface affects Tony's resting pose. Bottom means he is sitting
    // on the usable desktop/taskbar boundary; side edges make him lean inward.
    if(!dragging_) {
        switch(dockMode_) {
        case DockMode::Bottom: dy += 3; scale *= .995; break;
        case DockMode::Top: dy -= 2; rotation += 1.2*qSin(t*.30); break;
        case DockMode::Left: dx -= 3; rotation -= 2.4; break;
        case DockMode::Right: dx += 3; rotation += 2.4; break;
        case DockMode::Free: break;
        }
    }

    // Tony notices the cursor even while idle. With no dedicated eye sprite yet,
    // a tiny body lean gives the impression that he is following the user.
    if(!dragging_ && (action_==Action::Idle || action_==Action::Curious)) {
        const QPoint cursorLocal=mapFromGlobal(QCursor::pos());
        const QRect interest(-70,-70,width()+140,height()+140);
        if(interest.contains(cursorLocal)) {
            const qreal nx=qBound(-1.0,(cursorLocal.x()-width()/2.0)/(width()/2.0),1.0);
            dx += qRound(nx*3.0);
            rotation += nx*1.5;
        }
    }

    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0,0,0,38));
    const qreal shadowScale=(action_==Action::Celebrate ? .78 : action_==Action::Carried ? .62 : action_==Action::Walk ? .9 : 1.0);
    const qreal sw=112*shadowScale;
    p.drawEllipse(QRectF(width()/2.0-sw/2.0,196,sw,13));
    p.restore();

    const QPointF center(width()/2.0+dx,112+dy);
    const QSizeF size(200*scale,200*scale);
    QRectF target(QPointF(-size.width()/2.0,-size.height()/2.0),size);
    p.save();
    p.translate(center);
    p.rotate(rotation);
    if(const QPixmap *sprite=pixmapForAction(action_)) p.drawPixmap(target.toRect(),*sprite);
    else {
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
    if(a!=Action::Walk) { hasWalkTarget_=false; walkingOnWindow_=false; }
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
    if(n=="curious" || n=="look_mouse") return Action::Curious;
    if(n=="pet" || n=="petted") return Action::Pet;
    if(n=="dizzy") return Action::Dizzy;
    if(n=="stretch") return Action::Stretch;
    if(n=="yawn") return Action::Yawn;
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
        stepWalkAcrossDesktop();
    }
    const QPoint anchor=mapToGlobal(QPoint(width()/2,20));
    bubble_.follow(anchor);
    composer_.follow(anchor);
    if(action_!=Action::Idle || idleBlinking_) update();
}

void PetWindow::scheduleBlink(){
    if(blinkTimer_.isActive()) return;
    // Natural but quiet: roughly one blink every 5-11 seconds.
    blinkTimer_.start(QRandomGenerator::global()->bounded(5000,11001));
}

void PetWindow::scheduleIdleMoment(){
    // Check often enough to feel alive, but the behavior engine returns None most of the time.
    idleTimer_.start(QRandomGenerator::global()->bounded(22000,55001));
}

void PetWindow::runIdleMoment(){
    if(action_!=Action::Idle || dragging_ || agentState_!="idle" || composer_.isVisible()) {
        scheduleIdleMoment(); return;
    }

    if(edgePeek_ && (dockMode_==DockMode::Left || dockMode_==DockMode::Right || dockMode_==DockMode::Top) &&
       QRandomGenerator::global()->bounded(100)<26) {
        emotion_="curious";
        setAction(Action::Peek,1700);
        scheduleIdleMoment();
        return;
    }

    using Impulse=TonyBehaviorEngine::Impulse;
    const auto impulse=behavior_.chooseIdleImpulse(QTime::currentTime().hour());
    switch(impulse) {
    case Impulse::None:
        break;
    case Impulse::Sleep:
        emotion_="sleepy";
        setAction(Action::Sleep,QRandomGenerator::global()->bounded(7000,12001));
        break;
    case Impulse::Shiver:
        emotion_="cold";
        setAction(Action::Shiver,2300);
        if(QRandomGenerator::global()->bounded(100)<55)
  showBubble(uiText("Brrr... could I borrow a little warmth?","有点冷……可以借我一点温度吗？"),3600);
        break;
    case Impulse::AskHug:
        emotion_="hopeful";
        setAction(Action::AskHug,2800);
        if(QRandomGenerator::global()->bounded(100)<65)
  showBubble(uiText("Can I have a tiny hug?","可以抱我一下吗？就一下。"),3600);
        break;
    case Impulse::Wave:
        emotion_="friendly"; setAction(Action::Wave,1300); break;
    case Impulse::Walk:
        emotion_="playful";
        if(perchOnActiveWindow_) walkAlongForegroundWindow();
        else setAction(Action::Walk,3200);
        break;
    case Impulse::Study:
        emotion_="focused"; setAction(Action::Study,3000); break;
    case Impulse::AdjustGlasses:
        emotion_="focused"; setAction(Action::AdjustGlasses,1500); break;
    case Impulse::Stretch:
        emotion_="content"; setAction(Action::Stretch,1800); break;
    case Impulse::Yawn:
        emotion_="sleepy"; setAction(Action::Yawn,2200); break;
    case Impulse::RemoveGlasses:
        emotion_="confident"; setAction(Action::RemoveGlasses,2400); break;
    }
    scheduleIdleMoment();
}

void PetWindow::tickLife(){
    if(!lifeClock_.isValid()) lifeClock_.start();
    const qint64 elapsed=qMin<qint64>(lifeClock_.restart(),60000);
    const QPoint cursorLocal=mapFromGlobal(QCursor::pos());
    const QRect nearbyArea(-90,-90,width()+180,height()+180);
    const bool recentInteraction=activityClock_.isValid() && activityClock_.elapsed()<60000;
    const bool userNearby=nearbyArea.contains(cursorLocal) || recentInteraction;
    behavior_.tick(elapsed,agentState_!="idle",userNearby,QTime::currentTime().hour());

    if(++lifeSaveTicks_>=12) {
        QSettings s;
        behavior_.save(s);
        lifeSaveTicks_=0;
    }
    if((action_==Action::Idle || action_==Action::Curious) && nearbyArea.contains(cursorLocal)) update();
}


void PetWindow::tickDesktop(){
    if(dragging_ || mouseDown_ || falling_) return;

    updateForegroundWindowBehavior();
    if(hiddenForFullscreen_) return;
    ensureOnDesktop();

    const QPoint cursor=QCursor::pos();
    const QPoint previousCursor=lastCursorGlobal_;
    const int cursorTravel=(cursor-previousCursor).manhattanLength();
    if(cursorTravel<4) ++cursorStillTicks_;
    else cursorStillTicks_=0;
    lastCursorGlobal_=cursor;

    const bool idleAgent=agentState_=="idle" && !composer_.isVisible();
    const bool actionFree=(action_==Action::Idle || action_==Action::Curious) && !actionTimer_.isActive();

    // After ten quiet minutes Tony chooses the less intrusive lower corner and sleeps.
    if(autoRest_ && !autoRested_ && idleAgent && action_==Action::Idle &&
       activityClock_.isValid() && activityClock_.elapsed()>=10*60*1000) {
        moveToRestCorner();
        return;
    }

    if(perchOnActiveWindow_ && idleAgent && actionFree) {
        perchOnForegroundWindow();
    }

    if(!idleAgent || !actionFree || perchOnActiveWindow_) return;

    const auto life=behavior_.snapshot();
    const QPoint center=frameGeometry().center();
    const int dx=cursor.x()-center.x();
    const int dy=cursor.y()-center.y();
    const int distance=qAbs(dx)+qAbs(dy);
    const bool recentlyTouched=activityClock_.isValid() && activityClock_.elapsed()<2200;

    // A fast pointer sweep near Tony triggers an occasional short chase. Using the
    // swept rectangle catches passes that cross Tony even when both sampled endpoints
    // are already far away.
    QRect cursorSweep(previousCursor,cursor);
    cursorSweep=cursorSweep.normalized().adjusted(-105,-105,105,105);
    const bool sweptNear=cursorSweep.contains(center);
    if(fastCursorChase_ && followCursor_ && !recentlyTouched && cursorTravel>=150 && sweptNear &&
       life.curiosity>=52 && life.energy>=34 && QRandomGenerator::global()->bounded(100)<22) {
        startCursorWalk(cursor);
        cursorStillTicks_=0;
        return;
    }

    if(!followCursor_) return;

    // Four-Hz sensing means 12 still samples is roughly three seconds.
    if(cursorStillTicks_>=12 && !recentlyTouched && distance>150 && distance<520 &&
       life.curiosity>=58 && life.energy>=36 && QRandomGenerator::global()->bounded(100)<14) {
        startCursorWalk(cursor);
        cursorStillTicks_=0;
    }
}

bool PetWindow::foregroundWindowInfo(QRect *rect, QScreen **screenOut, bool *fullscreen) const {
    if(rect) *rect=QRect();
    if(screenOut) *screenOut=nullptr;
    if(fullscreen) *fullscreen=false;
#ifdef Q_OS_WIN
    HWND foreground=GetForegroundWindow();
    if(!foreground || !IsWindowVisible(foreground) || IsIconic(foreground)) return false;
    const HWND selfHandle=reinterpret_cast<HWND>(winId());
    if(foreground==selfHandle || foreground==GetShellWindow() || foreground==GetDesktopWindow()) return false;

    RECT wr{};
    if(!GetWindowRect(foreground,&wr) || wr.right<=wr.left || wr.bottom<=wr.top) return false;
    const QRect windowRect(wr.left,wr.top,wr.right-wr.left,wr.bottom-wr.top);
    QScreen *screen=screenForPoint(windowRect.center());
    if(!screen) return false;

    const QRect full=screen->geometry();
    constexpr int tolerance=8;
    const LONG_PTR style=GetWindowLongPtrW(foreground,GWL_STYLE);
    const bool borderless=(style & WS_CAPTION)==0 && (style & WS_THICKFRAME)==0;
    const bool coversMonitor=
        windowRect.left()<=full.left()+tolerance &&
        windowRect.top()<=full.top()+tolerance &&
        windowRect.right()-1>=full.right()-tolerance &&
        windowRect.bottom()-1>=full.bottom()-tolerance;
    // A maximized overlapped window can have invisible resize borders outside the
    // work area. Require a borderless foreground window as well, so normal maximized
    // browsers/editors never make Tony disappear.
    const bool isFullscreen=borderless && coversMonitor;

    if(rect) *rect=windowRect;
    if(screenOut) *screenOut=screen;
    if(fullscreen) *fullscreen=isFullscreen;
    return true;
#else
    return false;
#endif
}

void PetWindow::updateForegroundWindowBehavior(){
    QRect foregroundRect;
    QScreen *foregroundScreen=nullptr;
    bool fullscreen=false;
    const bool hasForeground=foregroundWindowInfo(&foregroundRect,&foregroundScreen,&fullscreen);

    if(hiddenForFullscreen_) {
        if(hideForFullscreen_ && hasForeground && fullscreen) return;
        hiddenForFullscreen_=false;
        show();
        raise();
        ensureOnDesktop();
        emotion_="content";
        setAction(Action::Land,650);
        tray_.setToolTip(agent_.connected() ? "Tony · connected" : "Tony · waiting for server");
        return;
    }

    if(hideForFullscreen_ && hasForeground && fullscreen && isVisible() && !composer_.isVisible()) {
        hiddenForFullscreen_=true;
        bubble_.dismiss();
        hide();
        tray_.setToolTip("Tony · resting during fullscreen");
    }
}

void PetWindow::perchOnForegroundWindow(){
    QRect windowRect;
    QScreen *screen=nullptr;
    bool fullscreen=false;
    if(!foregroundWindowInfo(&windowRect,&screen,&fullscreen) || !screen || fullscreen) return;

    const QRect area=screen->availableGeometry();
    const QRect visibleWindow=windowRect.intersected(area);
    if(visibleWindow.width()<180 || visibleWindow.height()<140) return;

    const int maxX=qMax(area.left(),area.right()-width()+1);
    const int maxY=qMax(area.top(),area.bottom()-height()+1);
    const int shadowY=196;
    const int rightInset=18;
    QPoint target;
    target.setX(qBound(area.left(),visibleWindow.right()-width()+1-rightInset,maxX));

    // Prefer sitting on the top edge when there is room above the app. Otherwise
    // Tony sits on the app's lower edge, clamped so the taskbar remains usable.
    const int topPerch=visibleWindow.top()-shadowY;
    const int bottomPerch=visibleWindow.bottom()-shadowY;
    target.setY(topPerch>=area.top() ? qMin(topPerch,maxY) : qBound(area.top(),bottomPerch,maxY));

    if((target-pos()).manhattanLength()>2) {
        move(target);
        bubble_.follow(mapToGlobal(QPoint(width()/2,20)));
        composer_.follow(mapToGlobal(QPoint(width()/2,20)));
    }
    dockMode_=DockMode::Free;
    dockScreenName_=screen->name();
}

void PetWindow::walkAlongForegroundWindow(){
    QRect windowRect;
    QScreen *screen=nullptr;
    bool fullscreen=false;
    if(!foregroundWindowInfo(&windowRect,&screen,&fullscreen) || !screen || fullscreen) {
        emotion_="curious";
        setAction(Action::Curious,900);
        return;
    }

    const QRect area=screen->availableGeometry();
    const QRect visibleWindow=windowRect.intersected(area);
    if(visibleWindow.width()<width()+80 || visibleWindow.height()<120) {
        emotion_="curious";
        setAction(Action::Curious,900);
        return;
    }

    const int shadowY=196;
    const int maxX=qMax(area.left(),area.right()-width()+1);
    const int topPerch=visibleWindow.top()-shadowY;
    const int bottomPerch=visibleWindow.bottom()-shadowY;
    const int y=topPerch>=area.top() ? qMin(topPerch,area.bottom()-height()+1)
                                     : qBound(area.top(),bottomPerch,area.bottom()-height()+1);
    const int leftX=qBound(area.left(),visibleWindow.left()+12,maxX);
    const int rightX=qBound(area.left(),visibleWindow.right()-width()+1-12,maxX);
    if(rightX-leftX<70) return;

    // Start from the nearest valid point on the same ledge, then head toward the
    // opposite half of the active window. This makes the movement read as walking
    // along a surface rather than teleporting between arbitrary desktop points.
    QPoint start=qBound(leftX,pos().x(),rightX)==pos().x() ? QPoint(pos().x(),y)
                                                          : QPoint(qBound(leftX,pos().x(),rightX),y);
    move(start);
    const int midpoint=(leftX+rightX)/2;
    const int targetX=start.x()<=midpoint ? rightX : leftX;
    windowWalkArea_=QRect(leftX,y,rightX-leftX+1,1);
    walkTarget_=QPoint(targetX,y);
    hasWalkTarget_=true;
    walkingOnWindow_=true;
    walkDirection_=targetX>=start.x() ? 1 : -1;
    emotion_="playful";
    setAction(Action::Walk,qBound(1800,qAbs(targetX-start.x())*12,7000));
    hasWalkTarget_=true;
    walkingOnWindow_=true;
    walkTarget_=QPoint(targetX,y);
}

bool PetWindow::wouldHitForegroundWindow(const QRect &nextFrame) const {
    if(walkingOnWindow_) return false;
    QRect obstacle;
    QScreen *screen=nullptr;
    bool fullscreen=false;
    if(!foregroundWindowInfo(&obstacle,&screen,&fullscreen) || fullscreen) return false;
    obstacle=obstacle.adjusted(-6,-6,6,6);
    const QRect currentBody=frameGeometry().adjusted(32,30,-32,-22);
    const QRect nextBody=nextFrame.adjusted(32,30,-32,-22);
    // If Tony already overlaps the active app, do not trap him there. Collision
    // avoidance only applies when a walk would newly enter the window.
    return !currentBody.intersects(obstacle) && nextBody.intersects(obstacle);
}

void PetWindow::startCursorWalk(const QPoint &cursor){
    QScreen *targetScreen=screenForPoint(cursor);
    if(!targetScreen) return;

    const QRect area=targetScreen->availableGeometry();
    const int maxX=qMax(area.left(),area.right()-width()+1);
    const int maxY=qMax(area.top(),area.bottom()-height()+1);
    QPoint target=pos();
    target.setX(qBound(area.left(),cursor.x()-width()/2,maxX));
    if(dockMode_==DockMode::Bottom) target.setY(maxY);
    else target.setY(qBound(area.top(),target.y(),maxY));

    const int horizontalDistance=qAbs(target.x()-pos().x());
    if(horizontalDistance<24) {
        emotion_="curious";
        setAction(Action::Curious,850);
        return;
    }

    walkTarget_=target;
    hasWalkTarget_=true;
    walkDirection_=target.x()>=pos().x() ? 1 : -1;
    if(dockMode_==DockMode::Top || dockMode_==DockMode::Left || dockMode_==DockMode::Right)
        dockMode_=DockMode::Free;
    emotion_="curious";
    setAction(Action::Walk,qBound(1000,(horizontalDistance*70)/5+500,6500));
    // setAction keeps explicit Walk targets; assign once more to make that invariant obvious.
    hasWalkTarget_=true;
    walkTarget_=target;
}

void PetWindow::startFall(bool rough){
    QScreen *screen=screenForPoint(frameGeometry().center());
    if(!screen) {
        settleOnDesktop();
        savePosition();
        emotion_=rough ? "dizzy" : "playful";
        setAction(rough ? Action::Dizzy : Action::Land, rough ? 1500 : 650);
        return;
    }

    const QRect area=screen->availableGeometry();
    QPoint p=pos();
    p.setX(qBound(area.left(),p.x(),qMax(area.left(),area.right()-width()+1)));
    p.setY(qMin(p.y(),area.bottom()-height()+1));
    move(p);

    dockMode_=DockMode::Free;
    dockScreenName_=screen->name();
    hasWalkTarget_=false;
    walkingOnWindow_=false;
    falling_=true;
    pendingDizzyAfterFall_=rough;
    fallVelocity_=0;
    bounceCount_=0;
    fallTargetY_=area.bottom()-height()+1;
    actionTimer_.stop();
    action_=Action::Carried;
    frame_=0;
    physicsTimer_.start();
    update();
}

void PetWindow::tickPhysics(){
    if(!falling_) {
        physicsTimer_.stop();
        return;
    }

    QScreen *screen=screenForPoint(frameGeometry().center());
    if(!screen) return;
    const QRect area=screen->availableGeometry();
    fallTargetY_=area.bottom()-height()+1;

    fallVelocity_ += 2;
    int nextY=pos().y()+fallVelocity_;
    if(nextY>=fallTargetY_) {
        move(pos().x(),fallTargetY_);
        if(bounceCount_<2 && qAbs(fallVelocity_)>=7) {
            ++bounceCount_;
            fallVelocity_=-qMax(3,qAbs(fallVelocity_)*35/100);
            action_=Action::Land;
            frame_=0;
            update();
            return;
        }

        falling_=false;
        physicsTimer_.stop();
        dockMode_=DockMode::Bottom;
        dockScreenName_=screen->name();
        savePosition();
        const bool dizzy=pendingDizzyAfterFall_;
        pendingDizzyAfterFall_=false;
        if(dizzy) {
            emotion_="dizzy";
            setAction(Action::Dizzy,1500);
            showBubble(uiText("Fast trip. My curls are still catching up.","飞得有点快，我的卷毛还没反应过来。"),3200);
        } else {
            emotion_="playful";
            setAction(Action::Land,700);
        }
        return;
    }

    move(pos().x(),nextY);
    const QPoint anchor=mapToGlobal(QPoint(width()/2,20));
    bubble_.follow(anchor);
    composer_.follow(anchor);
}

void PetWindow::moveToRestCorner(){
    QScreen *screen=screenForPoint(frameGeometry().center());
    if(!screen) screen=QGuiApplication::primaryScreen();
    if(!screen) return;

    const QRect area=screen->availableGeometry();
    const int leftX=area.left()+8;
    const int rightX=qMax(leftX,area.right()-width()+1-8);
    const int y=area.bottom()-height()+1;
    const QPoint cursor=QCursor::pos();
    const int leftDistance=qAbs(cursor.x()-(leftX+width()/2));
    const int rightDistance=qAbs(cursor.x()-(rightX+width()/2));
    const int x=leftDistance>rightDistance ? leftX : rightX;

    // Keep the user's active-window preference. Sleep blocks perching by itself;
    // after Tony wakes, the preference can resume naturally.
    dockMode_=DockMode::Bottom;
    dockScreenName_=screen->name();
    move(x,y);
    savePosition();
    autoRested_=true;
    emotion_="sleepy";
    setAction(Action::Sleep,0);
    tray_.setToolTip("Tony · sleeping in a quiet corner");
}

QScreen *PetWindow::screenForPoint(const QPoint &globalPoint) const {
    if(auto *screen=QGuiApplication::screenAt(globalPoint)) return screen;

    QScreen *best=QGuiApplication::primaryScreen();
    int bestDistance=std::numeric_limits<int>::max();
    for(auto *screen:QGuiApplication::screens()) {
        const QRect g=screen->geometry();
        const int dx=globalPoint.x()<g.left() ? g.left()-globalPoint.x() :
                     globalPoint.x()>g.right() ? globalPoint.x()-g.right() : 0;
        const int dy=globalPoint.y()<g.top() ? g.top()-globalPoint.y() :
                     globalPoint.y()>g.bottom() ? globalPoint.y()-g.bottom() : 0;
        const int d=dx+dy;
        if(d<bestDistance) { bestDistance=d; best=screen; }
    }
    return best;
}

QString PetWindow::dockModeName(DockMode mode) const {
    switch(mode) {
    case DockMode::Bottom:return "bottom";
    case DockMode::Top:return "top";
    case DockMode::Left:return "left";
    case DockMode::Right:return "right";
    case DockMode::Free:return "free";
    }
    return "free";
}

PetWindow::DockMode PetWindow::dockModeFromName(const QString &name) const {
    const QString n=name.trimmed().toLower();
    if(n=="bottom") return DockMode::Bottom;
    if(n=="top") return DockMode::Top;
    if(n=="left") return DockMode::Left;
    if(n=="right") return DockMode::Right;
    return DockMode::Free;
}

void PetWindow::ensureOnDesktop(){
    QScreen *screen=nullptr;
    if(!dockScreenName_.isEmpty()) {
        for(auto *candidate:QGuiApplication::screens()) {
            if(candidate->name()==dockScreenName_) { screen=candidate; break; }
        }
    }
    if(!screen) screen=screenForPoint(frameGeometry().center());
    if(!screen) return;

    const QRect area=screen->availableGeometry();
    QPoint p=pos();
    const int maxX=qMax(area.left(),area.right()-width()+1);
    const int maxY=qMax(area.top(),area.bottom()-height()+1);

    switch(dockMode_) {
    case DockMode::Bottom:
        p.setX(qBound(area.left(),p.x(),maxX));
        p.setY(maxY);
        break;
    case DockMode::Top:
        p.setX(qBound(area.left(),p.x(),maxX));
        p.setY(area.top());
        break;
    case DockMode::Left:
        p.setX(area.left());
        p.setY(qBound(area.top(),p.y(),maxY));
        break;
    case DockMode::Right:
        p.setX(maxX);
        p.setY(qBound(area.top(),p.y(),maxY));
        break;
    case DockMode::Free: {
        bool visible=false;
        for(auto *candidate:QGuiApplication::screens()) {
            const QRect intersection=frameGeometry().intersected(candidate->geometry());
            if(intersection.width()>=48 && intersection.height()>=48) { visible=true; break; }
        }
        if(!visible) {
            p.setX(qBound(area.left(),p.x(),maxX));
            p.setY(qBound(area.top(),p.y(),maxY));
        }
        break;
    }
    }

    if(p!=pos()) move(p);
    dockScreenName_=screen->name();
}

void PetWindow::settleOnDesktop(){
    QScreen *screen=screenForPoint(frameGeometry().center());
    if(!screen) return;
    dockScreenName_=screen->name();

    const QRect area=screen->availableGeometry();
    QPoint p=pos();
    const int maxX=qMax(area.left(),area.right()-width()+1);
    const int maxY=qMax(area.top(),area.bottom()-height()+1);
    p.setX(qBound(area.left(),p.x(),maxX));
    p.setY(qBound(area.top(),p.y(),maxY));

    dockMode_=DockMode::Free;
    if(snapToEdges_) {
        struct Candidate { int distance; DockMode mode; };
        const Candidate candidates[] = {
            {qAbs(p.y()+height()-1-area.bottom()),DockMode::Bottom},
            {qAbs(p.y()-area.top()),DockMode::Top},
            {qAbs(p.x()-area.left()),DockMode::Left},
            {qAbs(p.x()+width()-1-area.right()),DockMode::Right},
        };
        Candidate best=candidates[0];
        for(const auto &candidate:candidates) if(candidate.distance<best.distance) best=candidate;
        if(best.distance<=52) dockMode_=best.mode;
    }

    move(p);
    ensureOnDesktop();
}

void PetWindow::stepWalkAcrossDesktop(){
    QScreen *screen=QGuiApplication::screenAt(frameGeometry().center());
    if(!screen) screen=screenForPoint(frameGeometry().center());
    if(!screen) return;

    if(walkingOnWindow_) {
        const int remaining=walkTarget_.x()-pos().x();
        if(qAbs(remaining)<=4) {
            move(walkTarget_);
            hasWalkTarget_=false;
            walkingOnWindow_=false;
            actionTimer_.stop();
            emotion_="content";
            setAction(Action::Curious,900);
            return;
        }
        walkDirection_=remaining>0 ? 1 : -1;
        const int step=qMin(4,qAbs(remaining));
        QPoint next=pos()+QPoint(walkDirection_*step,0);
        next.setY(windowWalkArea_.top());
        next.setX(qBound(windowWalkArea_.left(),next.x(),windowWalkArea_.right()));
        move(next);
        dockScreenName_=screen->name();
        return;
    }

    if(hasWalkTarget_) {
        const int remaining=walkTarget_.x()-pos().x();
        if(qAbs(remaining)<=5) {
            move(walkTarget_);
            if(auto *arrived=screenForPoint(frameGeometry().center())) dockScreenName_=arrived->name();
            hasWalkTarget_=false;
            actionTimer_.stop();
            savePosition();
            restoreAgentAction();
            return;
        }
        walkDirection_=remaining>0 ? 1 : -1;
    }

    const QRect area=screen->availableGeometry();
    const int stepPixels=hasWalkTarget_ ? 5 : 2;
    QPoint next=pos()+QPoint(walkDirection_*stepPixels,0);
    const QRect nextFrame(next,QSize(width(),height()));

    if(wouldHitForegroundWindow(nextFrame)) {
        if(hasWalkTarget_) {
            hasWalkTarget_=false;
            actionTimer_.stop();
            emotion_="curious";
            setAction(Action::Curious,900);
        } else {
            walkDirection_=-walkDirection_;
            emotion_="curious";
        }
        return;
    }

    const int nextLeft=next.x();
    const int nextRight=next.x()+width()-1;
    if(nextLeft>=area.left() && nextRight<=area.right()) {
        move(next);
        dockScreenName_=screen->name();
        return;
    }

    QScreen *adjacent=nullptr;
    int bestGap=std::numeric_limits<int>::max();
    const QRect currentRect=frameGeometry();
    for(auto *candidate:QGuiApplication::screens()) {
        if(candidate==screen) continue;
        const QRect other=candidate->availableGeometry();
        const bool verticalOverlap=other.bottom()>=currentRect.top()+40 &&
                                   other.top()<=currentRect.bottom()-40;
        if(!verticalOverlap) continue;
        int gap=std::numeric_limits<int>::max();
        if(walkDirection_>0 && other.left()>=area.right()-2) gap=other.left()-area.right();
        if(walkDirection_<0 && other.right()<=area.left()+2) gap=area.left()-other.right();
        if(gap>=0 && gap<bestGap && gap<=96) { bestGap=gap; adjacent=candidate; }
    }

    if(adjacent) {
        const QRect other=adjacent->availableGeometry();
        next.setX(walkDirection_>0 ? other.left() : other.right()-width()+1);
        next.setY(qBound(other.top(),next.y(),qMax(other.top(),other.bottom()-height()+1)));
        move(next);
        dockScreenName_=adjacent->name();
        return;
    }

    if(hasWalkTarget_) {
        hasWalkTarget_=false;
        actionTimer_.stop();
        settleOnDesktop();
        restoreAgentAction();
        return;
    }

    walkDirection_=-walkDirection_;
    next=pos();
    next.setX(walkDirection_>0 ? area.left() : area.right()-width()+1);
    move(next);
}

void PetWindow::dockToTaskbar(QScreen *screen){
    if(!screen) screen=screenForPoint(frameGeometry().center());
    if(!screen) return;

    const QRect full=screen->geometry();
    const QRect area=screen->availableGeometry();
    const int insetLeft=area.left()-full.left();
    const int insetTop=area.top()-full.top();
    const int insetRight=full.right()-area.right();
    const int insetBottom=full.bottom()-area.bottom();

    int biggest=insetBottom;
    dockMode_=DockMode::Bottom;
    if(insetTop>biggest) { biggest=insetTop; dockMode_=DockMode::Top; }
    if(insetLeft>biggest) { biggest=insetLeft; dockMode_=DockMode::Left; }
    if(insetRight>biggest) { biggest=insetRight; dockMode_=DockMode::Right; }
    if(biggest<3) dockMode_=DockMode::Bottom;

    dockScreenName_=screen->name();
    QPoint p=pos();
    p.setX(qBound(area.left(),p.x(),qMax(area.left(),area.right()-width()+1)));
    p.setY(qBound(area.top(),p.y(),qMax(area.top(),area.bottom()-height()+1)));
    move(p);
    ensureOnDesktop();
    savePosition();
    emotion_="content";
    setAction(Action::Land,650);
}

void PetWindow::moveToNextScreen(){
    const auto screens=QGuiApplication::screens();
    if(screens.size()<2) {
        showBubble(uiText("I only see one display.","我现在只看到一个显示器。"),2600);
        return;
    }

    QScreen *current=screenForPoint(frameGeometry().center());
    int index=screens.indexOf(current);
    if(index<0) index=0;
    QScreen *next=screens.at((index+1)%screens.size());
    const QRect area=next->availableGeometry();
    move(area.center().x()-width()/2,area.bottom()-height()+1);
    dockScreenName_=next->name();
    dockMode_=DockMode::Bottom;
    ensureOnDesktop();
    savePosition();
    emotion_="playful";
    setAction(Action::Land,700);
}

void PetWindow::markInteraction(){
    if(activityClock_.isValid()) activityClock_.restart();
    else activityClock_.start();
    if(autoRested_) {
        autoRested_=false;
        tray_.setToolTip(agent_.connected() ? "Tony · connected" : "Tony · waiting for server");
        if(action_==Action::Sleep && agentState_=="idle") {
            emotion_="sleepy";
            setAction(Action::Yawn,1100);
        }
    }
}

bool PetWindow::isHeadHit(const QPoint &localPos) const {
    return QRect(width()/2-72,18,144,118).contains(localPos);
}

void PetWindow::handleTap(const QPoint &localPos){
    markInteraction();
    if(!rapidClickClock_.isValid() || rapidClickClock_.elapsed()>850) rapidClicks_=0;
    rapidClickClock_.restart();
    ++rapidClicks_;

    if(rapidClicks_>=4) {
        rapidClicks_=0;
        emotion_="dizzy";
        setAction(Action::Dizzy,1450);
        showBubble(uiText("Whoa... tiny paws need a second.","晕乎乎的……让我缓一下。"),2800);
        return;
    }

    if(isHeadHit(localPos)) {
        behavior_.onPetted();
        emotion_="happy";
        setAction(Action::Pet,900);
        if(QRandomGenerator::global()->bounded(100)<24)
  showBubble(uiText("Hehe. Head pats accepted.","嘿嘿，摸头批准。"),2400);
    } else {
        emotion_="curious";
        setAction(Action::Curious,850);
    }
}

void PetWindow::showLifeStatus(){
    const auto s=behavior_.snapshot();
    if(s.mood=="cold") {
        emotion_="cold"; setAction(Action::Shiver,1900);
        showBubble(uiText("I'm a little cold. A hug would fix that.","我有一点冷。抱一下大概就好了。"),4200);
    } else if(s.mood=="sleepy") {
        emotion_="sleepy"; setAction(Action::Yawn,1900);
        showBubble(uiText("A little sleepy... but I'm still here.","有一点困……不过我还在陪你。"),4200);
    } else if(s.mood=="cuddly") {
        emotion_="hopeful"; setAction(Action::AskHug,2200);
        showBubble(uiText("I may be in hug-request mode.","我现在可能处于求抱抱模式。"),4200);
    } else if(s.mood=="curious") {
        emotion_="curious"; setAction(Action::Curious,1600);
        showBubble(uiText("Curious. What are we working on?","有点好奇。我们今天在研究什么？"),4200);
    } else if(s.mood=="happy") {
        emotion_="happy"; setAction(Action::Wave,1400);
        showBubble(uiText("Pretty happy. Staying close.","挺开心的。就在你旁边待着。"),4200);
    } else {
        emotion_="content"; setAction(Action::Stretch,1500);
        showBubble(uiText("I'm good. Just keeping you company.","我挺好的，就在桌面上陪你。"),4200);
    }
}

QString PetWindow::actionName() const {
    switch(action_){
    case Action::Idle:return "idle"; case Action::Curious:return "curious"; case Action::Peek:return "peeking from the edge"; case Action::Pet:return "being petted";
    case Action::Carried:return "being carried"; case Action::Land:return "landing"; case Action::Dizzy:return "dizzy";
    case Action::Stretch:return "stretching"; case Action::Yawn:return "yawning";
    case Action::Bob:return "working"; case Action::Walk:return "walking";
    case Action::Think:return "thinking"; case Action::Celebrate:return "celebrating"; case Action::Sleep:return "sleeping";
    case Action::Shiver:return "shivering"; case Action::AskHug:return "asking for a hug"; case Action::Hug:return "hugging";
    case Action::Blush:return "blushing"; case Action::BlushWave:return "blushing for Paula"; case Action::Study:return "studying";
    case Action::AdjustGlasses:return "adjusting glasses"; case Action::RemoveGlasses:return "no-glasses mode"; case Action::Wave:return "waving";
    }
    return "idle";
}

void PetWindow::enterEvent(QEnterEvent*){
    hovered_=true;
    if(agentState_=="idle" && action_==Action::Idle) {
        if(!blinkTimer_.isActive()) scheduleBlink();
        hoverTimer_.start(700);
    }
    update();
}

void PetWindow::leaveEvent(QEvent*){
    hovered_=false;
    hoverTimer_.stop();
    update();
}

void PetWindow::mousePressEvent(QMouseEvent *e){
    if(e->button()!=Qt::LeftButton) return;
    // Tony can be grabbed again while he is falling; user input always wins over physics.
    if(falling_) {
        falling_=false;
        pendingDizzyAfterFall_=false;
        physicsTimer_.stop();
    }
    mouseDown_=true;
    dragging_=false;
    dragTravel_=0;
    pressGlobal_=e->globalPosition().toPoint();
    lastDragGlobal_=pressGlobal_;
    dragOffset_=pressGlobal_-frameGeometry().topLeft();
    pressClock_.restart();
    markInteraction();
}

void PetWindow::mouseMoveEvent(QMouseEvent *e){
    const QPoint global=e->globalPosition().toPoint();
    if(mouseDown_ && (e->buttons()&Qt::LeftButton)) {
        if(!dragging_ && (global-pressGlobal_).manhattanLength()>=QApplication::startDragDistance()) {
          dragging_=true;
          if(perchOnActiveWindow_) {
              perchOnActiveWindow_=false;
              QSettings().setValue("desktop/perch_on_active_window",false);
          }
          hasWalkTarget_=false;
          dockMode_=DockMode::Free;
          dockScreenName_.clear();
          actionTimer_.stop();
  action_=Action::Carried;
  frame_=0;
  lastDragGlobal_=global;
        }
        if(dragging_) {
  dragTravel_ += (global-lastDragGlobal_).manhattanLength();
  lastDragGlobal_=global;
  move(global-dragOffset_);
  const QPoint anchor=mapToGlobal(QPoint(width()/2,20));
  bubble_.follow(anchor);
  composer_.follow(anchor);
        }
    }
    update();
}

void PetWindow::mouseReleaseEvent(QMouseEvent *e){
    if(e->button()!=Qt::LeftButton || !mouseDown_) return;
    mouseDown_=false;
    if(dragging_) {
        const bool rough=dragTravel_>850 || (pressClock_.isValid() && pressClock_.elapsed()<450 && dragTravel_>360);
        behavior_.onDragged(rough);
        dragging_=false;
        if(gravityEnabled_) {
            startFall(rough);
        } else {
            settleOnDesktop();
            savePosition();
            if(rough) {
                emotion_="dizzy";
                setAction(Action::Dizzy,1500);
                showBubble(uiText("Fast trip. My curls are still catching up.","飞得有点快，我的卷毛还没反应过来。"),3200);
            } else {
                emotion_="playful";
                setAction(Action::Land,650);
            }
        }
    } else {
        handleTap(e->position().toPoint());
    }
}

void PetWindow::mouseDoubleClickEvent(QMouseEvent *e){
    if(e->button()==Qt::LeftButton) {
        rapidClicks_=0;
        askTony();
    }
}

void PetWindow::contextMenuEvent(QContextMenuEvent *e){
    markInteraction();
    QMenu m;
    auto ask=m.addAction(uiText("Chat with Tony…","和 Tony 聊天…"));
    auto hug=m.addAction(uiText("Hug Tony","抱抱 Tony"));
    auto feeling=m.addAction(uiText("How are you feeling?","Tony 现在怎么样？"));
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

    auto *desktopMenu=settings->addMenu(uiText("Desktop behavior","桌面行为"));
    auto followCursor=desktopMenu->addAction(uiText("Gently follow a nearby cursor","轻轻跟随附近的鼠标"));
    followCursor->setCheckable(true);
    followCursor->setChecked(followCursor_);
    auto snapEdges=desktopMenu->addAction(uiText("Snap to screen edges / taskbar","靠近屏幕边缘 / 任务栏时停靠"));
    snapEdges->setCheckable(true);
    snapEdges->setChecked(snapToEdges_);
    auto fullscreenAware=desktopMenu->addAction(uiText("Hide during fullscreen apps","全屏应用时自动躲起来"));
    fullscreenAware->setCheckable(true);
    fullscreenAware->setChecked(hideForFullscreen_);
    auto perchWindow=desktopMenu->addAction(uiText("Perch on the active window edge","坐在当前窗口边缘"));
    perchWindow->setCheckable(true);
    perchWindow->setChecked(perchOnActiveWindow_);
    auto gravity=desktopMenu->addAction(uiText("Gravity and bounce after dragging","拖起来后有重力下落和弹跳"));
    gravity->setCheckable(true);
    gravity->setChecked(gravityEnabled_);
    auto fastChase=desktopMenu->addAction(uiText("React to fast cursor sweeps","鼠标快速掠过时会追一下"));
    fastChase->setCheckable(true);
    fastChase->setChecked(fastCursorChase_);
    auto edgePeek=desktopMenu->addAction(uiText("Peek from screen edges","停在屏幕边缘时会探头"));
    edgePeek->setCheckable(true);
    edgePeek->setChecked(edgePeek_);
    auto autoRest=desktopMenu->addAction(uiText("Sleep in a corner after 10 quiet minutes","10 分钟无人操作后去角落睡觉"));
    autoRest->setCheckable(true);
    autoRest->setChecked(autoRest_);
    desktopMenu->addSeparator();
    auto walkWindow=desktopMenu->addAction(uiText("Walk along the active window","沿当前窗口边缘走一走"));
    auto walkToCursor=desktopMenu->addAction(uiText("Walk to the cursor now","现在走到鼠标旁边"));
    auto taskbarHome=desktopMenu->addAction(uiText("Sit by the taskbar","回到任务栏旁边"));
    auto nextDisplay=desktopMenu->addAction(uiText("Move to next display","去下一个显示器"));

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
    else if(chosen==feeling) showLifeStatus();
    else if(chosen==paula) { behavior_.onPaulaMention(); markInteraction(); emotion_="bashful"; setAction(Action::BlushWave,2600); showBubble(uiText("Paula? Wait—do I look okay?","Paula？等等——我看起来还好吗？"),4200); }
    else if(chosen==pair) configureConnection();
    else if(chosen==english || chosen==chinese) {
        const QString lang=(chosen==chinese) ? "zh" : "en";
        applyUiLanguage(lang);
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
    else if(chosen==followCursor) {
        followCursor_=followCursor->isChecked();
        QSettings().setValue("desktop/follow_cursor",followCursor_);
        showBubble(followCursor_ ? uiText("Okay. I may wander over when the cursor waits nearby.","好。我看到鼠标在附近停着时，偶尔会走过去看看。") : uiText("Okay. I will stay put unless you move me.","好。我不会主动追着鼠标跑了。"),3200);
    }
    else if(chosen==snapEdges) {
        snapToEdges_=snapEdges->isChecked();
        QSettings().setValue("desktop/snap_to_edges",snapToEdges_);
        if(snapToEdges_) settleOnDesktop();
        else { dockMode_=DockMode::Free; savePosition(); }
    }
    else if(chosen==fullscreenAware) {
        hideForFullscreen_=fullscreenAware->isChecked();
        QSettings().setValue("desktop/hide_for_fullscreen",hideForFullscreen_);
        if(!hideForFullscreen_ && hiddenForFullscreen_) { hiddenForFullscreen_=false; show(); ensureOnDesktop(); }
    }
    else if(chosen==perchWindow) {
        perchOnActiveWindow_=perchWindow->isChecked();
        QSettings().setValue("desktop/perch_on_active_window",perchOnActiveWindow_);
        if(perchOnActiveWindow_) { hasWalkTarget_=false; perchOnForegroundWindow(); }
        else savePosition();
    }
    else if(chosen==gravity) {
        gravityEnabled_=gravity->isChecked();
        QSettings().setValue("desktop/gravity",gravityEnabled_);
    }
    else if(chosen==fastChase) {
        fastCursorChase_=fastChase->isChecked();
        QSettings().setValue("desktop/fast_cursor_chase",fastCursorChase_);
    }
    else if(chosen==edgePeek) {
        edgePeek_=edgePeek->isChecked();
        QSettings().setValue("desktop/edge_peek",edgePeek_);
    }
    else if(chosen==autoRest) {
        autoRest_=autoRest->isChecked();
        QSettings().setValue("desktop/auto_rest",autoRest_);
        if(!autoRest_) autoRested_=false;
    }
    else if(chosen==walkWindow) walkAlongForegroundWindow();
    else if(chosen==walkToCursor) { perchOnActiveWindow_=false; QSettings().setValue("desktop/perch_on_active_window",false); startCursorWalk(QCursor::pos()); }
    else if(chosen==taskbarHome) { perchOnActiveWindow_=false; QSettings().setValue("desktop/perch_on_active_window",false); dockToTaskbar(); }
    else if(chosen==nextDisplay) { perchOnActiveWindow_=false; QSettings().setValue("desktop/perch_on_active_window",false); moveToNextScreen(); }
    else if(chosen==idle) setAction(Action::Idle);
    else if(chosen==walk) {
        if(perchOnActiveWindow_) walkAlongForegroundWindow();
        else { if(dockMode_==DockMode::Left || dockMode_==DockMode::Right || dockMode_==DockMode::Top) dockMode_=DockMode::Free; setAction(Action::Walk,3000); }
    }
    else if(chosen==glasses) setAction(Action::AdjustGlasses,1600);
    else if(chosen==noGlasses) setAction(Action::RemoveGlasses,2600);
    else if(chosen==cold) { emotion_="cold"; setAction(Action::Shiver,2200); showBubble(uiText("Brrr… warm paws, please.","好冷……给我暖暖爪子。"),3600); }
    else if(chosen==sleep) setAction(Action::Sleep);
    else if(chosen==quit) qApp->quit();
}

void PetWindow::askTony(){
    markInteraction();
    bubble_.dismiss(); emotion_="curious";
    if(agentState_=="idle") setAction(Action::Think,0);
    composer_.openAt(mapToGlobal(QPoint(width()/2,40)));
}

void PetWindow::submitTonyPrompt(const QString &text){
    const QString prompt=text.trimmed();
    if(prompt.isEmpty()) return;
    markInteraction();
    behavior_.onConversation();
    if(prompt.contains("paula",Qt::CaseInsensitive)) behavior_.onPaulaMention();
    answer_.clear(); emotion_="curious"; agentState_="thinking"; restoreAgentAction();
    if(agent_.connected()) agent_.sendMessage(prompt);
    else {
        agentState_="idle"; setAction(Action::Think,2800);
        showBubble(uiText("Tony is not connected yet. Right-click me and choose Connect to Tony.\n\n", "Tony 还没有连接。右键点我并选择“连接 Tony”。\n\n")+prompt,6500);
    }
}

void PetWindow::hugTony(){ markInteraction(); behavior_.onHugged(); emotion_="happy"; setAction(Action::Hug,2600); showBubble(uiText("Got you. Tony cuddles closer.","抱到啦。Tony 开心地靠近了一点。"),4300); }

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
    else { auto a=QGuiApplication::primaryScreen()->availableGeometry(); move(a.right()-width()-40,a.bottom()-height()+1); }
}
void PetWindow::savePosition(){
    QSettings s;
    s.setValue("pet/position",pos());
    s.setValue("pet/dock_mode",dockModeName(dockMode_));
    s.setValue("pet/dock_screen",dockScreenName_);
}
