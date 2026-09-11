#include "SpeechBubble.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

namespace {
QColor bubbleFill(const QString &tone) {
    const auto t=tone.toLower();
    if(t.contains("bashful") || t.contains("happy") || t.contains("warm") || t.contains("hopeful"))
        return QColor(255,247,249,248);
    if(t.contains("cold") || t.contains("worried"))
        return QColor(246,250,255,248);
    if(t.contains("focused") || t.contains("curious"))
        return QColor(250,249,246,248);
    return QColor(255,255,255,248);
}

QColor bubbleStroke(const QString &tone) {
    const auto t=tone.toLower();
    if(t.contains("bashful") || t.contains("happy") || t.contains("warm") || t.contains("hopeful"))
        return QColor(226,169,180);
    if(t.contains("cold") || t.contains("worried"))
        return QColor(160,184,210);
    if(t.contains("focused") || t.contains("curious"))
        return QColor(181,173,157);
    return QColor(185,185,185);
}
}

SpeechBubble::SpeechBubble(QWidget *parent): QWidget(parent) {
    setWindowFlags(Qt::ToolTip|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hideTimer_.setSingleShot(true);
    connect(&hideTimer_, &QTimer::timeout, this, &SpeechBubble::hide);
}

void SpeechBubble::showMessage(const QString &text, const QPoint &anchorGlobal, const QString &tone, int timeoutMs) {
    text_=text.left(900);
    tone_=tone;
    anchorGlobal_=anchorGlobal;
    updateGeometryForText();
    placeNear(anchorGlobal_);
    show();
    raise();
    if(timeoutMs>0) hideTimer_.start(timeoutMs); else hideTimer_.stop();
    update();
}

void SpeechBubble::follow(const QPoint &anchorGlobal) {
    anchorGlobal_=anchorGlobal;
    if(isVisible()) placeNear(anchorGlobal_);
}

void SpeechBubble::dismiss() {
    hideTimer_.stop();
    hide();
}

void SpeechBubble::updateGeometryForText() {
    QFont font;
    font.setFamily("Microsoft YaHei UI");
    font.setPointSize(10);
    QFontMetrics fm(font);
    const int maxTextWidth=340;
    QRect r=fm.boundingRect(QRect(0,0,maxTextWidth,1000),Qt::TextWordWrap|Qt::AlignLeft|Qt::AlignTop,text_);
    const int w=qBound(150,r.width()+34,374);
    const int h=qBound(54,r.height()+34,230);
    resize(w,h+12);
}

void SpeechBubble::placeNear(const QPoint &anchorGlobal) {
    auto *screen=QGuiApplication::screenAt(anchorGlobal);
    if(!screen) screen=QGuiApplication::primaryScreen();
    QRect area=screen ? screen->availableGeometry() : QRect(anchorGlobal-QPoint(500,400),QSize(1000,800));

    int x=anchorGlobal.x()-width()/2;
    int y=anchorGlobal.y()-height()-8;
    if(y<area.top()+8) y=anchorGlobal.y()+18;
    x=qBound(area.left()+8,x,area.right()-width()-8);
    y=qBound(area.top()+8,y,area.bottom()-height()-8);
    move(x,y);
}

void SpeechBubble::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,true);

    const QRectF body(2,2,width()-4,height()-16);
    p.setPen(QPen(bubbleStroke(tone_),1.4));
    p.setBrush(bubbleFill(tone_));
    p.drawRoundedRect(body,14,14);

    const qreal cx=width()/2.0;
    QPainterPath tail;
    tail.moveTo(cx-9,height()-15);
    tail.lineTo(cx,height()-3);
    tail.lineTo(cx+9,height()-15);
    tail.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(bubbleFill(tone_));
    p.drawPath(tail);

    p.setPen(QColor(42,42,42));
    QFont font;
    font.setFamily("Microsoft YaHei UI");
    font.setPointSize(10);
    p.setFont(font);
    p.drawText(QRect(18,14,width()-36,height()-34),Qt::TextWordWrap|Qt::AlignLeft|Qt::AlignTop,text_);
}
