#include "PetWindow.h"

#include <QCursor>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

namespace {
constexpr int kTonyCanvasWidth = 320;
constexpr int kTonyCanvasHeight = 250;
constexpr qreal kCanvasSideSafety = 6.0;
constexpr qreal kCanvasTopSafety = 5.0;
constexpr qreal kCanvasBottomSafety = 6.0;
constexpr qreal kTonyGroundY = 205.0;
constexpr qreal kTonyShadowY = 196.0;
}

void PetWindow::paintEvent(QPaintEvent*) {
    // 1.0.8 still hosted every pose in a 280 px wide QWidget and then imposed a
    // second 252x192 visible-content box. In 1.0.9 the QWidget itself is the
    // overscan canvas. Keep the established vertical desktop geometry so gravity,
    // taskbar docking and perching stay compatible, while adding horizontal room
    // for wide paws, props and rotated poses.
    if(width()!=kTonyCanvasWidth || height()!=kTonyCanvasHeight) {
        const QPoint oldCenter=frameGeometry().center();
        setFixedSize(kTonyCanvasWidth,kTonyCanvasHeight);
        move(oldCenter.x()-width()/2,pos().y());
        ensureOnDesktop();
        update();
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.setRenderHint(QPainter::SmoothPixmapTransform,true);

    const qreal t=frame_/8.0;
    int dx=0, dy=0;
    qreal scale=1.0, rotation=0.0;
    switch(action_) {
    case Action::Idle:
        if(idleBlinking_) { dy=1; scale=.995; }
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
    case Action::HeadTilt: dx=int(2*qSin(t*.4)); rotation=5.5*qSin(t*.55); scale=1.008; break;
    case Action::Nod: dy=int(3*qSin(t*1.4)); scale=1.0+0.008*qSin(t*1.4); break;
    case Action::Paw: dy=-qAbs(int(2*qSin(t*1.7))); rotation=4.2*qSin(t*1.7); scale=1.012; break;
    case Action::Hop: dy=-qAbs(int(10*qSin(t*1.15))); scale=1.0+0.025*qSin(t*1.15); rotation=1.5*qSin(t*.9); break;
    case Action::Spin: rotation=(frame_%40)*9.0; scale=.985; break;
    case Action::Sniff: dx=int(3*qSin(t*1.2)); dy=qAbs(int(2*qSin(t*.8))); rotation=-2.0+1.5*qSin(t*.9); break;
    case Action::Dance: dx=int(5*qSin(t*1.1)); dy=-qAbs(int(5*qSin(t*1.7))); rotation=5.0*qSin(t*1.1); scale=1.01+0.015*qSin(t*1.4); break;
    }

    if(!dragging_) {
        switch(dockMode_) {
        case DockMode::Bottom: dy += 3; scale *= .995; break;
        case DockMode::Top: dy -= 2; rotation += 1.2*qSin(t*.30); break;
        case DockMode::Left: dx -= 3; rotation -= 2.4; break;
        case DockMode::Right: dx += 3; rotation += 2.4; break;
        case DockMode::Free: break;
        }
    }

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
    p.drawEllipse(QRectF(width()/2.0-sw/2.0,kTonyShadowY,sw,13));
    p.restore();

    p.save();
    if(const QPixmap *sprite=pixmapForAction(action_); sprite && !sprite->isNull()) {
        // Alpha bounds are used only to find the visual center. The source QRect
        // below always covers 100% of the PNG canvas: no sourceRect cropping,
        // no automatic trim, and no old 252x192 visible-content box.
        const QImage image=sprite->toImage().convertToFormat(QImage::Format_ARGB32);
        int minX=image.width(), minY=image.height(), maxX=-1, maxY=-1;
        for(int y=0; y<image.height(); ++y) {
            const QRgb *line=reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for(int x=0; x<image.width(); ++x) {
                if(qAlpha(line[x])>0) {
                    minX=qMin(minX,x); minY=qMin(minY,y);
                    maxX=qMax(maxX,x); maxY=qMax(maxY,y);
                }
            }
        }
        if(maxX<minX || maxY<minY) {
            minX=0; minY=0; maxX=qMax(0,image.width()-1); maxY=qMax(0,image.height()-1);
        }

        const qreal visibleW=qMax(1,maxX-minX+1);
        const qreal visibleH=qMax(1,maxY-minY+1);
        const qreal visibleCx=(minX+maxX+1)/2.0;
        const qreal visibleCy=(minY+maxY+1)/2.0;

        const qreal safeLeft=kCanvasSideSafety;
        const qreal safeRight=width()-kCanvasSideSafety;
        const qreal safeTop=kCanvasTopSafety;
        const qreal safeBottom=height()-kCanvasBottomSafety;
        const qreal safeWidth=qMax<qreal>(1.0,safeRight-safeLeft);
        const qreal safeHeight=qMax<qreal>(1.0,safeBottom-safeTop);

        // Native-size-first rendering: keep the authored PNG at 1:1 whenever
        // possible. Animation scale is no longer clamped to the old 0.97-1.02
        // range. We only scale down when actual rotation would cross the physical
        // QWidget boundary; that is a safety condition, not an artistic crop.
        const qreal nativeFit=qMin<qreal>(1.0,qMin(safeWidth/visibleW,safeHeight/visibleH));
        const qreal requestedFit=nativeFit*scale;
        const qreal radians=qDegreesToRadians(rotation);
        const qreal absCos=qAbs(qCos(radians));
        const qreal absSin=qAbs(qSin(radians));
        const qreal rotatedUnitW=visibleW*absCos+visibleH*absSin;
        const qreal rotatedUnitH=visibleH*absCos+visibleW*absSin;
        const qreal boundaryFit=qMin(
            safeWidth/qMax<qreal>(1.0,rotatedUnitW),
            safeHeight/qMax<qreal>(1.0,rotatedUnitH));
        const qreal fit=qMin(requestedFit,boundaryFit);

        const qreal rotatedW=rotatedUnitW*fit;
        const qreal rotatedH=rotatedUnitH*fit;
        const qreal desiredCenterX=width()/2.0+dx;
        const qreal desiredCenterY=(kTonyGroundY+dy)-visibleH*fit/2.0;
        const qreal minCenterX=safeLeft+rotatedW/2.0;
        const qreal maxCenterX=safeRight-rotatedW/2.0;
        const qreal minCenterY=safeTop+rotatedH/2.0;
        const qreal maxCenterY=safeBottom-rotatedH/2.0;
        const qreal centerX=(minCenterX<=maxCenterX)
            ? qBound(minCenterX,desiredCenterX,maxCenterX)
            : (safeLeft+safeRight)/2.0;
        const qreal centerY=(minCenterY<=maxCenterY)
            ? qBound(minCenterY,desiredCenterY,maxCenterY)
            : (safeTop+safeBottom)/2.0;

        p.translate(QPointF(centerX,centerY));
        p.rotate(rotation);
        if(action_==Action::Walk && walkDirection_>0) p.scale(-1.0,1.0);
        const QRectF target(-visibleCx*fit,-visibleCy*fit,image.width()*fit,image.height()*fit);
        p.drawPixmap(target,*sprite,QRectF(0,0,sprite->width(),sprite->height()));
    } else {
        p.translate(QPointF(width()/2.0+dx,108+dy));
        p.rotate(rotation);
        p.setBrush(QColor(246,220,181));
        p.setPen(QPen(QColor(70,45,35),3));
        p.drawEllipse(QRectF(-75,-75,150,150));
        p.setPen(QColor(35,35,35));
        QFont fallbackFont=p.font();
        fallbackFont.setBold(true);
        fallbackFont.setPointSize(18);
        p.setFont(fallbackFont);
        p.drawText(QRectF(-95,-30,190,60),Qt::AlignCenter,"TONY");
    }
    p.restore();

    // The old bottom name plate consumed 34 px of the image-safe region. Tony's
    // identity is already present in tray/chat UI, so the pet canvas is now fully
    // reserved for artwork instead of drawing a label over/under the sprite.
}
