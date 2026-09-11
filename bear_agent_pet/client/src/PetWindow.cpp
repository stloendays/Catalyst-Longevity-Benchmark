#include "PetWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QInputDialog>
#include <QSettings>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QRandomGenerator>
#include <QToolTip>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QLineEdit>
#include <QtMath>

PetWindow::PetWindow(QWidget *parent): QWidget(parent) {
    setWindowTitle("Tony");
    setFixedSize(230,250);
    setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint|Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    loadAsset();
    restorePosition();

    animTimer_.setInterval(40);
    connect(&animTimer_, &QTimer::timeout, this, &PetWindow::tickAnimation);
    animTimer_.start();

    // Passive personality should feel alive without interrupting the user constantly.
    idleTimer_.setInterval(22000);
    connect(&idleTimer_, &QTimer::timeout, this, [this]{
        if(action_!=Action::Idle || dragging_ || agentState_!="idle") return;
        const int r=QRandomGenerator::global()->bounded(12);
        if(r==0) {
            setAction(Action::Shiver,2600);
            showBubble("Brrr... it's a little cold here.");
        } else if(r==1) {
            setAction(Action::AskHug,3200);
            showBubble("Can I have a tiny hug?");
        } else if(r==2) {
            setAction(Action::Walk,5000);
        } else if(r==3) {
            setAction(Action::Think,2200);
        } else if(r==4) {
            setAction(Action::Wave,1700);
        }
    });
    idleTimer_.start();

    actionTimer_.setSingleShot(true);
    connect(&actionTimer_, &QTimer::timeout, this, &PetWindow::restoreAgentAction);

    connect(&agent_, &AgentClient::stateChanged, this, [this](const QString &s){
        agentState_=s.trimmed().toLower();
        // Temporary persona actions (hug, shiver, blush, etc.) take precedence.
        // Once their timer expires, restoreAgentAction() returns Tony to the
        // correct long-lived Agent phase instead of incorrectly dropping to idle.
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
        showBubble(answer_);
    });
    connect(&agent_, &AgentClient::answerFinished, this, [this]{
        if(!actionTimer_.isActive()) setAction(Action::Celebrate,1600);
    });
    connect(&agent_, &AgentClient::connectionChanged, this, [this](bool connected){
        if(!connected) agentState_="idle";
        tray_.setToolTip(connected ? "Tony · connected" : "Tony · waiting for server");
    });
    connect(&agent_, &AgentClient::errorMessage, this, [this](const QString &text){
        agentState_="error";
        emotion_="worried";
        setAction(Action::Think,2600);
        showBubble("Tony couldn't finish that: " + text.left(260));
    });
    connect(&tunnel_, &SshTunnel::statusChanged, this, [this](const QString &status){
        if(!agent_.connected()) tray_.setToolTip("Tony · "+status);
    });

    tray_.setToolTip("Tony · Desktop Agent");
    tray_.setVisible(true);
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r){
        if(r==QSystemTrayIcon::Trigger){ show(); raise(); }
    });

    QSettings s;
    const auto endpoint=s.value("agent/url","ws://127.0.0.1:18790/agent/ws").toUrl();
    // The gateway intentionally stays bound to server loopback. When Tony uses
    // the default localhost endpoint, open a local SSH forward using the user's
    // existing Windows OpenSSH key/agent. No private key is embedded in the app.
    if(endpoint.host()=="127.0.0.1" || endpoint.host()=="localhost") tunnel_.start();
    agent_.connectTo(endpoint);
}

PetWindow::~PetWindow() {
    tunnel_.stop();
}

void PetWindow::loadAsset() {
    QStringList candidates{
        QCoreApplication::applicationDirPath()+"/assets/tony.png",
        QCoreApplication::applicationDirPath()+"/tony.png",
        QDir::currentPath()+"/assets/tony.png"
    };
    for(const auto &p:candidates) if(QFileInfo::exists(p) && pet_.load(p)) return;
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
        dy=int(6*qSin(t*1.7));
        break;
    case Action::Walk:
        dy=-qAbs(int(3*qSin(t*2.2)));
        rotation=1.8*qSin(t*2.2);
        break;
    case Action::Think:
        dy=int(2*qSin(t));
        rotation=-2.0+1.0*qSin(t*.7);
        scale=1.0+0.012*qSin(t*.8);
        break;
    case Action::Celebrate:
        dy=-qAbs(int(12*qSin(t*1.8)));
        scale=1.0+0.03*qSin(t*1.8);
        rotation=3.0*qSin(t*1.8);
        break;
    case Action::Sleep:
        dy=int(1*qSin(t*.35));
        scale=.99+0.008*qSin(t*.35);
        rotation=-2.0;
        break;
    case Action::Shiver:
        dx=(frame_%4<2)?-3:3;
        dy=int(qSin(t));
        scale=.99;
        break;
    case Action::AskHug:
        dy=-qAbs(int(4*qSin(t*1.2)));
        scale=1.02+0.025*qSin(t*.9);
        rotation=1.5*qSin(t*.7);
        break;
    case Action::Hug:
        scale=1.07+0.025*qSin(t*.8);
        dy=-3;
        rotation=2.0*qSin(t*.65);
        break;
    case Action::Blush:
        dy=int(2*qSin(t*.8));
        rotation=2.5*qSin(t*.55);
        scale=1.015;
        break;
    case Action::Study:
        dy=int(3*qSin(t*1.1));
        rotation=-1.2;
        break;
    case Action::AdjustGlasses:
        dy=-qAbs(int(3*qSin(t*1.6)));
        rotation=-2.5*qSin(t*1.2);
        break;
    case Action::RemoveGlasses:
        scale=1.045+0.012*qSin(t*.8);
        rotation=-1.5;
        break;
    case Action::Wave:
        dy=-qAbs(int(4*qSin(t*1.6)));
        rotation=4.0*qSin(t*1.8);
        break;
    }

    const QPointF center(width()/2.0+dx,112+dy);
    const QSizeF size(200*scale,200*scale);
    QRectF target(QPointF(-size.width()/2.0,-size.height()/2.0),size);

    p.save();
    p.translate(center);
    p.rotate(rotation);
    if(!pet_.isNull()) {
        p.drawPixmap(target.toRect(),pet_);
    } else {
        p.setBrush(QColor(246,220,181));
        p.setPen(QPen(QColor(70,45,35),3));
        p.drawEllipse(QRectF(-75,-75,150,150));
        p.setPen(QColor(35,35,35));
        QFont f=p.font(); f.setBold(true); f.setPointSize(18); p.setFont(f);
        p.drawText(QRectF(-95,-30,190,60),Qt::AlignCenter,"TONY");
    }
    p.restore();

    p.setPen(QColor(255,255,255,235));
    QFont f=p.font(); f.setBold(true); f.setPointSize(12); p.setFont(f);
    p.drawText(QRect(0,210,width(),25),Qt::AlignCenter,"Tony");
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
    if(n=="blush" || n=="blush_wave" || n=="offer_scarf") return Action::Blush;
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
    update();
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
    case Action::Study:return "studying";
    case Action::AdjustGlasses:return "adjusting glasses";
    case Action::RemoveGlasses:return "no-glasses mode";
    case Action::Wave:return "waving";
    }
    return "idle";
}

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
void PetWindow::mouseDoubleClickEvent(QMouseEvent *e){ if(e->button()==Qt::LeftButton) askTony(); }

void PetWindow::contextMenuEvent(QContextMenuEvent *e){
    QMenu m;
    auto ask=m.addAction("问 Tony…");
    auto hug=m.addAction("抱抱 Tony");
    m.addSeparator();
    auto idle=m.addAction("坐好");
    auto walk=m.addAction("散步");
    auto think=m.addAction("思考");
    auto study=m.addAction("学习化学");
    auto glasses=m.addAction("扶一下眼镜");
    auto noGlasses=m.addAction("摘掉眼镜耍帅");
    auto celebrate=m.addAction("开心一下");
    auto cold=m.addAction("有点冷");
    auto sleep=m.addAction("睡觉");
    m.addSeparator();
    auto quit=m.addAction("退出 Tony");

    auto chosen=m.exec(e->globalPos());
    if(chosen==ask) askTony();
    else if(chosen==hug) hugTony();
    else if(chosen==idle) setAction(Action::Idle);
    else if(chosen==walk) setAction(Action::Walk,6000);
    else if(chosen==think) setAction(Action::Think,4000);
    else if(chosen==study) setAction(Action::Study,5000);
    else if(chosen==glasses) setAction(Action::AdjustGlasses,2200);
    else if(chosen==noGlasses) setAction(Action::RemoveGlasses,3500);
    else if(chosen==celebrate) setAction(Action::Celebrate,2200);
    else if(chosen==cold) { setAction(Action::Shiver,3000); showBubble("Brrr... warm paws, please."); }
    else if(chosen==sleep) setAction(Action::Sleep);
    else if(chosen==quit) qApp->quit();
}

void PetWindow::askTony(){
    bool ok=false;
    auto text=QInputDialog::getText(this,"Tony","想让我做什么？",QLineEdit::Normal,{},&ok);
    if(!ok||text.trimmed().isEmpty()) return;
    answer_.clear();
    agentState_="thinking";
    restoreAgentAction();
    if(agent_.connected()) agent_.sendMessage(text);
    else showBubble("服务器还没连接好，我先在这里等你。\n\n"+text);
}

void PetWindow::hugTony(){
    emotion_="happy";
    setAction(Action::Hug,3000);
    showBubble("抱到啦。*Tony 开心地蹭了蹭* ");
}

void PetWindow::showBubble(const QString &text){
    QToolTip::showText(mapToGlobal(QPoint(width()/2,-10)),text.left(700),this);
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
void PetWindow::savePosition(){ QSettings().setValue("pet/position",pos()); }
