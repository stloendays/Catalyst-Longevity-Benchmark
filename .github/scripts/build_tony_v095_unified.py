#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CLIENT = ROOT / "bear_agent_pet" / "client"
CPP = CLIENT / "src" / "PetWindowV7.cpp"
HEADER = CLIENT / "src" / "PetWindow.h"
CMAKE = CLIENT / "CMakeLists.txt"
VERSION = CLIENT / "VERSION"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"Missing merge anchor for {label}: {old[:140]!r}")
    return text.replace(old, new, 1)


def replace_function(text: str, signature: str, replacement: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise SystemExit(f"Could not find function: {signature}")
    brace = text.find("{", start)
    if brace < 0:
        raise SystemExit(f"Could not find function body: {signature}")
    depth = 0
    end = None
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    if end is None:
        raise SystemExit(f"Could not find function end: {signature}")
    return text[:start] + replacement.rstrip() + "\n" + text[end:]


def patch_header() -> None:
    text = HEADER.read_text(encoding="utf-8")
    required = (
        'TonyBehaviorEngine.h', 'enum class DockMode', 'Curious', 'Peek', 'Pet',
        'Carried', 'Land', 'Dizzy', 'Stretch', 'Yawn', 'tickPhysics',
        'walkAlongForegroundWindow', 'syncOperatorLogsForUpdate',
    )
    missing = [token for token in required if token not in text]
    if missing:
        raise SystemExit("Behavior-rich header is incomplete: " + ", ".join(missing))
    HEADER.write_text(text, encoding="utf-8")


def patch_cmake() -> None:
    text = CMAKE.read_text(encoding="utf-8")
    if "src/TonyBehaviorEngine.cpp" not in text:
        text = replace_once(
            text,
            "    src/PetWindow.h\n",
            "    src/PetWindow.h\n    src/TonyBehaviorEngine.cpp\n    src/TonyBehaviorEngine.h\n",
            "TonyBehaviorEngine sources",
        )
    CMAKE.write_text(text, encoding="utf-8")


def patch_constructor_and_anchors(text: str) -> str:
    text = replace_once(
        text,
        '    setFixedSize(230,250);\n',
        '    // Full-exposure host: preserve Tony\'s complete artwork, including wide poses.\n'
        '    setFixedSize(280,250);\n',
        "full-exposure host width",
    )

    old_connection = '''    if(!token.isEmpty()) {\n        if(endpoint.host()=="127.0.0.1" || endpoint.host()=="localhost") tunnel_.start();\n        agent_.connectTo(endpoint,token);\n    } else {\n        tray_.setToolTip("Tony · not paired");\n        showBubble(uiText("Hi Paula. Right-click me and choose Connect to Tony.","嗨 Paula。右键点我，然后选择“连接 Tony”。"),6500);\n    }\n'''
    new_connection = '''    const QString visualTestAction=qEnvironmentVariable("TONY_VISUAL_ACTION").trimmed();\n    if(!token.isEmpty()) {\n        if(endpoint.host()=="127.0.0.1" || endpoint.host()=="localhost") tunnel_.start();\n        agent_.connectTo(endpoint,token);\n    } else {\n        tray_.setToolTip("Tony · not paired");\n        if(visualTestAction.isEmpty())\n            showBubble(uiText("Hi Paula. Right-click me and choose Connect to Tony.","嗨 Paula。右键点我，然后选择“连接 Tony”。"),6500);\n    }\n\n    // CI-only pose mode: no dialog/bubble may cover Tony during visual checks.\n    if(!visualTestAction.isEmpty()) {\n        bubble_.dismiss();\n        action_=actionFromWire(visualTestAction);\n        frame_=0;\n        update();\n    }\n'''
    if "TONY_VISUAL_ACTION" not in text:
        text = replace_once(text, old_connection, new_connection, "visual test mode")

    # Full-exposure main moved chat/bubble anchors to the very top of the wider host.
    text = text.replace("QPoint(width()/2,20)", "QPoint(width()/2,4)")
    return text


FULL_EXPOSURE_PAINT = r'''void PetWindow::paintEvent(QPaintEvent*) {
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
    p.drawEllipse(QRectF(width()/2.0-sw/2.0,196,sw,13));
    p.restore();

    p.save();
    if(const QPixmap *sprite=pixmapForAction(action_); sprite && !sprite->isNull()) {
        // Never crop Tony's source. Alpha bounds are measurement only; the
        // complete PNG canvas is drawn so ears, paws and props remain exposed.
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
        const QRectF target(-visibleCx*fit,-visibleCy*fit,image.width()*fit,image.height()*fit);
        p.drawPixmap(target,*sprite,QRectF(0,0,sprite->width(),sprite->height()));
    } else {
        p.translate(QPointF(width()/2.0+dx,108+dy));
        p.rotate(rotation);
        p.setBrush(QColor(246,220,181));
        p.setPen(QPen(QColor(70,45,35),3));
        p.drawEllipse(QRectF(-75,-75,150,150));
        p.setPen(QColor(35,35,35));
        QFont fallbackFont=p.font(); fallbackFont.setBold(true); fallbackFont.setPointSize(18); p.setFont(fallbackFont);
        p.drawText(QRectF(-95,-30,190,60),Qt::AlignCenter,"TONY");
    }
    p.restore();

    const QRect namePlate(width()/2-35,218,70,23);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20,20,20,105));
    p.drawRoundedRect(namePlate,11,11);
    p.setPen(QColor(255,255,255,245));
    QFont nameFont("Microsoft YaHei UI",10,QFont::DemiBold);
    p.setFont(nameFont);
    p.drawText(namePlate,Qt::AlignCenter,"Tony");
}'''


def patch_cpp() -> None:
    text = CPP.read_text(encoding="utf-8")
    text = patch_constructor_and_anchors(text)
    text = replace_function(text, "void PetWindow::paintEvent(QPaintEvent*)", FULL_EXPOSURE_PAINT)
    CPP.write_text(text, encoding="utf-8")


def verify() -> None:
    header = HEADER.read_text(encoding="utf-8")
    cpp = CPP.read_text(encoding="utf-8")
    cmake = CMAKE.read_text(encoding="utf-8")
    checks = {
        "life engine": "TonyBehaviorEngine behavior_" in header,
        "physics": "void PetWindow::tickPhysics()" in cpp,
        "gravity": "startFall" in cpp and "gravityEnabled_" in header,
        "window walking": "void PetWindow::walkAlongForegroundWindow()" in cpp,
        "foreground collision": "wouldHitForegroundWindow" in cpp,
        "hardened fullscreen": "borderless && coversMonitor" in cpp,
        "fast cursor chase": "cursorTravel>=150" in cpp,
        "auto rest": "moveToRestCorner" in cpp,
        "edge peek": "Action::Peek" in cpp,
        "multi screen": "moveToNextScreen" in cpp,
        "full exposure width": "setFixedSize(280,250)" in cpp,
        "full canvas draw": "QRectF(0,0,sprite->width(),sprite->height())" in cpp,
        "visual CI mode": "TONY_VISUAL_ACTION" in cpp,
        "settings/log bridge": "syncOperatorLogsForUpdate" in header,
        "engine build": "src/TonyBehaviorEngine.cpp" in cmake,
    }
    missing = [name for name, ok in checks.items() if not ok]
    if missing:
        raise SystemExit("Unified Tony verification failed: " + ", ".join(missing))


def main() -> int:
    patch_header()
    patch_cmake()
    patch_cpp()
    VERSION.write_text("0.9.5\n", encoding="utf-8")
    verify()
    print("TONY_V095_UNIFIED_STATIC=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
