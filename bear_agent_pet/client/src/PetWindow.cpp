#include "PetWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QScreen>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QToolTip>
#include <QFileInfo>
#include <QDir>
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

    idleTimer_.setInterval(6500);
    connect(&idleTimer_, &QTimer::timeout, this, [this]{
        if(action_!=Action::Idle || dragging_) return;
        const int r=QRandomGenerator::global()->bounded(4);
        setAction(r==0?Action::Bob:r==1?Action::Walk:r==2?Action::Think:Action::Idle, r==3?0:1800);
    });
    idleTimer_.start();
    connect(&actionTimer_, &QTimer::timeout, this, [this]{ setAction(Action::Idle); });
    actionTimer_.setSingleShot(true);

    connect(&agent_, &AgentClient::stateChanged, this, [this](const QString &s){
        if(s=="thinking") setAction(Action::Think);
        else if(s=="working" || s=="tool_running") setAction(Action::Bob);
        else if(s=="sleeping") setAction(Action::Sleep);
        else if(s=="idle") setAction(Action::Idle);
    });
    connect(&agent_, &AgentClient::textDelta, this, [this](const QString &t){ answer_ += t; showBubble(answer_); });
    connect(&agent_, &AgentClient::answerFinished, this, [this]{ setAction(Action::Celebrate,1600); });

    QSettings s;
    const auto endpoint=s.value("agent/url","ws://127.0.0.1:18790/ws").toUrl();
    agent_.connectTo(endpoint);

    tray_.setToolTip("Tony - Desktop Agent");
    tray_.setVisible(true);
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r){ if(r==QSystemTrayIcon::Trigger){ show(); raise(); }});
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
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing,true); p.setRenderHint(QPainter::SmoothPixmapTransform,true);
    const qreal t=frame_/8.0;
    int dy=0; qreal scale=1.0;
    if(action_==Action::Idle) dy=int(2*qSin(t));
    if(action_==Action::Bob) dy=int(7*qSin(t*1.7));
    if(action_==Action::Think) { dy=int(2*qSin(t)); scale=1.0+0.015*qSin(t*.8); }
    if(action_==Action::Celebrate) { dy=-qAbs(int(12*qSin(t*1.8))); scale=1.0+0.03*qSin(t*1.8); }
    if(action_==Action::Sleep) dy=int(1*qSin(t*.4));

    QRectF r(15,10+dy,200*scale,200*scale);
    r.moveCenter(QPointF(width()/2.0,112+dy));
    if(!pet_.isNull()) p.drawPixmap(r.toRect(),pet_);
    else {
        p.setBrush(QColor(246,220,181)); p.setPen(QPen(QColor(70,45,35),3));
        p.drawEllipse(QRectF(40,30+dy,150,150));
        p.setPen(QColor(35,35,35)); QFont f=p.font(); f.setBold(true); f.setPointSize(18); p.setFont(f);
        p.drawText(QRectF(20,65+dy,190,60),Qt::AlignCenter,"TONY");
    }
    p.setPen(QColor(255,255,255,230)); QFont f=p.font(); f.setBold(true); f.setPointSize(12); p.setFont(f);
    p.drawText(QRect(0,210,width(),30),Qt::AlignCenter,"Tony");
}

void PetWindow::setAction(Action a,int durationMs){ action_=a; frame_=0; basePos_=pos(); if(durationMs>0) actionTimer_.start(durationMs); else actionTimer_.stop(); update(); }
void PetWindow::tickAnimation(){
    ++frame_;
    if(action_==Action::Walk && !dragging_){
        auto screen=QGuiApplication::screenAt(frameGeometry().center()); if(!screen) screen=QGuiApplication::primaryScreen();
        auto area=screen->availableGeometry(); int dx=(frame_/6)%2?2:1; QPoint n=pos()+QPoint(dx,0);
        if(n.x()+width()>area.right()) n.setX(area.left()); move(n);
    }
    update();
}
QString PetWindow::actionName() const { switch(action_){case Action::Idle:return "idle";case Action::Bob:return "working";case Action::Walk:return "walking";case Action::Think:return "thinking";case Action::Celebrate:return "celebrating";case Action::Sleep:return "sleeping";} return "idle"; }
void PetWindow::mousePressEvent(QMouseEvent *e){ if(e->button()==Qt::LeftButton){ dragging_=true; dragOffset_=e->globalPosition().toPoint()-frameGeometry().topLeft(); }}
void PetWindow::mouseMoveEvent(QMouseEvent *e){ if(dragging_ && (e->buttons()&Qt::LeftButton)){ move(e->globalPosition().toPoint()-dragOffset_); setAction(Action::Bob); }}
void PetWindow::mouseReleaseEvent(QMouseEvent *e){ if(e->button()==Qt::LeftButton){ dragging_=false; savePosition(); setAction(Action::Idle); }}
void PetWindow::mouseDoubleClickEvent(QMouseEvent *e){ if(e->button()==Qt::LeftButton) askTony(); }
void PetWindow::contextMenuEvent(QContextMenuEvent *e){
    QMenu m; auto ask=m.addAction("问 Tony…"); m.addSeparator();
    auto idle=m.addAction("坐好"); auto walk=m.addAction("散步"); auto think=m.addAction("思考"); auto celebrate=m.addAction("开心一下"); auto sleep=m.addAction("睡觉");
    m.addSeparator(); auto quit=m.addAction("退出 Tony");
    auto chosen=m.exec(e->globalPos());
    if(chosen==ask) askTony(); else if(chosen==idle) setAction(Action::Idle); else if(chosen==walk) setAction(Action::Walk,6000); else if(chosen==think) setAction(Action::Think,4000); else if(chosen==celebrate) setAction(Action::Celebrate,2200); else if(chosen==sleep) setAction(Action::Sleep); else if(chosen==quit) qApp->quit();
}
void PetWindow::askTony(){
    bool ok=false; auto text=QInputDialog::getText(this,"Tony","想让我做什么？",QLineEdit::Normal,{},&ok); if(!ok||text.trimmed().isEmpty()) return;
    answer_.clear(); setAction(Action::Think); if(agent_.connected()) agent_.sendMessage(text); else showBubble("服务器还没连接好，我先在这里等你。\n\n"+text);
}
void PetWindow::showBubble(const QString &text){ QToolTip::showText(mapToGlobal(QPoint(width()/2,-10)),text.left(500),this); }
void PetWindow::restorePosition(){ QSettings s; auto v=s.value("pet/position"); if(v.isValid()) move(v.toPoint()); else { auto a=QGuiApplication::primaryScreen()->availableGeometry(); move(a.right()-width()-40,a.bottom()-height()-20); }}
void PetWindow::savePosition(){ QSettings().setValue("pet/position",pos()); }
