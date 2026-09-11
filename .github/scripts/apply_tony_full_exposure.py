from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CPP = ROOT / "bear_agent_pet/client/src/PetWindowV7.cpp"
BUBBLE = ROOT / "bear_agent_pet/client/src/SpeechBubble.cpp"
COMPOSER = ROOT / "bear_agent_pet/client/src/ChatComposer.cpp"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def regex_once(text: str, pattern: str, replacement: str, label: str) -> str:
    out, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one regex match, found {count}")
    return out


cpp = CPP.read_text(encoding="utf-8")
cpp = replace_once(cpp, "    setFixedSize(230,250);", "    // Give wide poses (study/working/sleep) enough transparent room to stay full-size.\n    // The artwork itself is never cropped; only the visible alpha bounds are used for scale/anchor math.\n    setFixedSize(280,250);", "window width")

old_connection = '''    if(!token.isEmpty()) {
        if(endpoint.host()=="127.0.0.1" || endpoint.host()=="localhost") tunnel_.start();
        agent_.connectTo(endpoint,token);
    } else {
        tray_.setToolTip("Tony · not paired");
        showBubble(uiText("Hi Paula. Right-click me and choose Connect to Tony.","嗨 Paula。右键点我，然后选择“连接 Tony”。"),6500);
    }
}'''
new_connection = '''    const QString visualTestAction=qEnvironmentVariable("TONY_VISUAL_ACTION").trimmed();
    if(!token.isEmpty()) {
        if(endpoint.host()=="127.0.0.1" || endpoint.host()=="localhost") tunnel_.start();
        agent_.connectTo(endpoint,token);
    } else {
        tray_.setToolTip("Tony · not paired");
        if(visualTestAction.isEmpty())
            showBubble(uiText("Hi Paula. Right-click me and choose Connect to Tony.","嗨 Paula。右键点我，然后选择“连接 Tony”。"),6500);
    }

    // Optional CI-only visual mode. It lets the Windows screenshot job render
    // every pose without dialogs or bubbles covering Tony.
    if(!visualTestAction.isEmpty()) {
        bubble_.dismiss();
        action_=actionFromWire(visualTestAction);
        frame_=0;
        update();
    }
}'''
cpp = replace_once(cpp, old_connection, new_connection, "visual test mode")

paint = r'''void PetWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.setRenderHint(QPainter::SmoothPixmapTransform,true);

    const qreal t=frame_/8.0;
    int dx=0, dy=0;
    qreal scale=1.0, rotation=0.0;
    switch(action_) {
    case Action::Idle:
        // Keep the approved idle artwork unchanged. A tiny timed settle gives
        // life without inventing a new face or changing Tony's character design.
        if(idleBlinking_) { dy=1; scale=.995; }
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
        // IMPORTANT: never crop Tony's source artwork. We inspect alpha bounds
        // only to calculate a stable visible size and a common ground line, then
        // draw the ENTIRE original PNG canvas. Transparent pixels may extend
        // outside the widget, but every visible character/drawing pixel remains exposed.
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
        if(maxX<minX || maxY<minY) {
            minX=0; minY=0; maxX=qMax(0,image.width()-1); maxY=qMax(0,image.height()-1);
        }

        const int visibleW=qMax(1,maxX-minX+1);
        const int visibleH=qMax(1,maxY-minY+1);
        const qreal maxMotionScale=(action_==Action::Hug) ? 1.035 : 1.02;
        const qreal safeMotionScale=qBound<qreal>(0.97,scale,maxMotionScale);

        // 280 px host width is deliberate: wide states such as study/working no
        // longer have to shrink by ~25% just because books/screens are present.
        // Height remains capped so Tony never collides with the nameplate.
        const qreal maxVisibleW=252.0;
        const qreal maxVisibleH=192.0;
        const qreal baseFit=qMin(maxVisibleW/visibleW,maxVisibleH/visibleH);
        const qreal fit=baseFit*safeMotionScale;

        const qreal visibleCx=(minX+maxX+1)/2.0;
        const qreal visibleCy=(minY+maxY+1)/2.0;
        const qreal groundY=205.0+dy;
        const qreal pivotY=groundY-visibleH*fit/2.0;

        p.translate(QPointF(width()/2.0+dx,pivotY));
        p.rotate(rotation);
        if(action_==Action::Walk && walkDirection_<0) p.scale(-1.0,1.0);

        // Full-canvas target: source rect is the complete PNG, never alpha-cropped.
        const QRectF target(-visibleCx*fit,-visibleCy*fit,
                            image.width()*fit,image.height()*fit);
        p.drawPixmap(target,*sprite,QRectF(0,0,sprite->width(),sprite->height()));
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

void PetWindow::setAction'''
cpp = regex_once(cpp, r'void PetWindow::paintEvent\(QPaintEvent\*\) \{.*?\n\}\n\nvoid PetWindow::setAction', paint, "paintEvent")

cpp = cpp.replace('mapToGlobal(QPoint(width()/2,20))', 'mapToGlobal(QPoint(width()/2,4))')
cpp = replace_once(cpp, 'composer_.openAt(mapToGlobal(QPoint(width()/2,40)));', 'composer_.openAt(mapToGlobal(QPoint(width()/2,4)));', 'composer anchor')
cpp = replace_once(cpp, 'void PetWindow::showBubble(const QString &text, int timeoutMs){ bubble_.showMessage(text,mapToGlobal(QPoint(width()/2,20)),emotion_,timeoutMs); }', 'void PetWindow::showBubble(const QString &text, int timeoutMs){ bubble_.showMessage(text,mapToGlobal(QPoint(width()/2,4)),emotion_,timeoutMs); }', 'bubble anchor') if 'mapToGlobal(QPoint(width()/2,20))' in cpp else cpp

old_restore = '''void PetWindow::restorePosition(){
    QSettings s; auto v=s.value("pet/position");
    if(v.isValid()) move(v.toPoint());
    else { auto a=QGuiApplication::primaryScreen()->availableGeometry(); move(a.right()-width()-40,a.bottom()-height()-20); }
}'''
new_restore = '''void PetWindow::restorePosition(){
    QSettings s;
    const auto v=s.value("pet/position");
    QPoint target;
    if(v.isValid()) target=v.toPoint();
    else {
        const auto a=QGuiApplication::primaryScreen()->availableGeometry();
        target=QPoint(a.right()-width()-39,a.bottom()-height()-19);
    }

    auto *screen=QGuiApplication::screenAt(target+QPoint(width()/2,height()/2));
    if(!screen) screen=QGuiApplication::primaryScreen();
    const QRect area=screen->availableGeometry();
    target.setX(qBound(area.left(),target.x(),area.right()-width()+1));
    target.setY(qBound(area.top(),target.y(),area.bottom()-height()+1));
    move(target);
}'''
cpp = replace_once(cpp, old_restore, new_restore, "restorePosition")
cpp = cpp.replace('n.setX(area.right()-width());', 'n.setX(area.right()-width()+1);')
CPP.write_text(cpp, encoding="utf-8")

bubble = BUBBLE.read_text(encoding="utf-8")
new_bubble_place = r'''void SpeechBubble::placeNear(const QPoint &anchorGlobal) {
    auto *screen=QGuiApplication::screenAt(anchorGlobal);
    if(!screen) screen=QGuiApplication::primaryScreen();
    const QRect area=screen ? screen->availableGeometry() : QRect(anchorGlobal-QPoint(500,400),QSize(1000,800));

    // Prefer above Tony with a real gap. If the pet is at the top edge, move the
    // bubble to a side instead of placing it over the character.
    const int edge=8;
    const int petHalfWidthPlusGap=156; // 280/2 + 16 px breathing room
    int x=anchorGlobal.x()-width()/2;
    int y=anchorGlobal.y()-height()-14;

    if(y<area.top()+edge) {
        const int leftX=anchorGlobal.x()-petHalfWidthPlusGap-width();
        const int rightX=anchorGlobal.x()+petHalfWidthPlusGap;
        if(leftX>=area.left()+edge) x=leftX;
        else if(rightX+width()<=area.right()-edge) x=rightX;
        else x=qBound(area.left()+edge,x,area.right()-width()-edge);
        y=qBound(area.top()+edge,anchorGlobal.y()+8,area.bottom()-height()-edge);
    }

    x=qBound(area.left()+edge,x,area.right()-width()-edge);
    y=qBound(area.top()+edge,y,area.bottom()-height()-edge);
    move(x,y);
}'''
bubble = regex_once(bubble, r'void SpeechBubble::placeNear\(const QPoint &anchorGlobal\) \{.*?\n\}', new_bubble_place, "SpeechBubble::placeNear")
BUBBLE.write_text(bubble, encoding="utf-8")

composer = COMPOSER.read_text(encoding="utf-8")
new_composer_place = r'''void ChatComposer::placeNear(const QPoint &anchorGlobal) {
    auto *screen=QGuiApplication::screenAt(anchorGlobal);
    if(!screen) screen=QGuiApplication::primaryScreen();
    const QRect area=screen ? screen->availableGeometry() : QRect(anchorGlobal-QPoint(600,400),QSize(1200,800));

    const int h=sizeHint().height()>0 ? sizeHint().height() : 92;
    resize(width(),h);

    const int edge=8;
    const int petHalfWidthPlusGap=156;
    int x=anchorGlobal.x()-width()/2;
    int y=anchorGlobal.y()-height()-14;

    // Never fall back directly on top of Tony. Near the top of the screen the
    // composer moves to whichever side has room.
    if(y<area.top()+edge) {
        const int leftX=anchorGlobal.x()-petHalfWidthPlusGap-width();
        const int rightX=anchorGlobal.x()+petHalfWidthPlusGap;
        if(leftX>=area.left()+edge) x=leftX;
        else if(rightX+width()<=area.right()-edge) x=rightX;
        else x=qBound(area.left()+edge,x,area.right()-width()-edge);
        y=qBound(area.top()+edge,anchorGlobal.y()+8,area.bottom()-height()-edge);
    }

    x=qBound(area.left()+edge,x,area.right()-width()-edge);
    y=qBound(area.top()+edge,y,area.bottom()-height()-edge);
    move(x,y);
}'''
composer = regex_once(composer, r'void ChatComposer::placeNear\(const QPoint &anchorGlobal\) \{.*?\n\}', new_composer_place, "ChatComposer::placeNear")
COMPOSER.write_text(composer, encoding="utf-8")

print("Tony full-exposure patch applied")
