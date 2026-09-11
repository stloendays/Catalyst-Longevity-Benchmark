#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CPP = ROOT / "bear_agent_pet/client/src/PetWindowV7.cpp"
CMAKE = ROOT / "bear_agent_pet/client/CMakeLists.txt"
BOOTSTRAP = ROOT / ".github/tony_assets_bootstrap/chunk_00"
BOOTSTRAP_NOTE = ROOT / ".github/tony_assets_bootstrap/README_FIX.txt"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"Missing patch anchor: {label}")
    return text.replace(old, new, 1)


cpp = CPP.read_text(encoding="utf-8")

old_picker = r'''const QPixmap *PetWindow::pixmapForAction(Action action) const {
    const QString key=assetKeyForAction(action);
    auto framesIt=animationAssets_.constFind(key);

    // Idle is not a looping animation. Keep the normal pose fixed and only
    // play the existing idle face frames during the short blink window.
    if(action==Action::Idle) {
        if(idleBlinking_ && framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty()) {
            static constexpr int blinkSequence[] = {0, 1, 2, 2, 1, 0, 0};
            const int sequenceSize=static_cast<int>(sizeof(blinkSequence)/sizeof(blinkSequence[0]));
            const int step=qBound(0,idleBlinkTick_,sequenceSize-1);
            const int index=blinkSequence[step] % framesIt.value().size();
            return &framesIt.value().at(index);
        }
        auto idleIt=stateAssets_.constFind("idle");
        if(idleIt!=stateAssets_.constEnd()) return &idleIt.value();
        if(framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty())
            return &framesIt.value().first();
    }

    if(framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty()) {
        const int stride=qMax(1,frameStrideForAction(action));
        const int index=(frame_/stride)%framesIt.value().size();
        return &framesIt.value().at(index);
    }
    auto it=stateAssets_.constFind(key);
    if(it!=stateAssets_.constEnd()) return &it.value();
    it=stateAssets_.constFind("idle");
    if(it!=stateAssets_.constEnd()) return &it.value();
    return pet_.isNull() ? nullptr : &pet_;
}
'''

new_picker = r'''const QPixmap *PetWindow::pixmapForAction(Action action) const {
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
'''
cpp = replace_once(cpp, old_picker, new_picker, "stable state-art picker")

old_draw = r'''    const QPointF center(width()/2.0+dx,112+dy);
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
'''

new_draw = r'''    const QPointF center(width()/2.0+dx,108+dy);
    p.save();
    p.translate(center);
    p.rotate(rotation);
    if(const QPixmap *sprite=pixmapForAction(action_); sprite && !sprite->isNull()) {
        // Fit the visible (alpha) subject, not the square PNG canvas. Different
        // source files can have different transparent padding; drawing every
        // canvas into a hard-coded 200x200 box made the character jump in size
        // and could push limbs outside the tiny desktop window.
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

        const qreal safeMotionScale=qBound<qreal>(0.965,scale,1.02);
        const qreal maxW=184.0*safeMotionScale;
        const qreal maxH=194.0*safeMotionScale;
        const qreal fit=qMin(maxW/qMax(1,source.width()),
                             maxH/qMax(1,source.height()));
        const QSizeF drawSize(source.width()*fit,source.height()*fit);
        const QRectF target(-drawSize.width()/2.0,-drawSize.height()/2.0,
                            drawSize.width(),drawSize.height());
        p.drawPixmap(target,*sprite,QRectF(source));
    } else {
        p.setBrush(QColor(246,220,181));
        p.setPen(QPen(QColor(70,45,35),3));
        p.drawEllipse(QRectF(-75,-75,150,150));
        p.setPen(QColor(35,35,35));
        QFont f=p.font(); f.setBold(true); f.setPointSize(18); p.setFont(f);
        p.drawText(QRectF(-95,-30,190,60),Qt::AlignCenter,"TONY");
    }
    p.restore();
'''
cpp = replace_once(cpp, old_draw, new_draw, "alpha-safe full-character renderer")
CPP.write_text(cpp, encoding="utf-8")

cmake = CMAKE.read_text(encoding="utf-8")
if "project(TonyDesktopPet VERSION 0.9.0 LANGUAGES CXX)" in cmake:
    cmake = cmake.replace(
        "project(TonyDesktopPet VERSION 0.9.0 LANGUAGES CXX)",
        "project(TonyDesktopPet VERSION 0.9.1 LANGUAGES CXX)",
        1,
    )
elif "project(TonyDesktopPet VERSION 0.9.1 LANGUAGES CXX)" not in cmake:
    raise SystemExit("Unexpected Tony version; refusing an unsafe version rewrite")
CMAKE.write_text(cmake, encoding="utf-8")

# The v0.10 bootstrap was truncated (not a valid ZIP). Remove it so it cannot
# silently reintroduce partial artwork into a later Windows release.
for path in (BOOTSTRAP, BOOTSTRAP_NOTE):
    if path.exists():
        path.unlink()

print("TONY_VISUAL_REGRESSION_FIX=PASS")
print("- static complete state art preferred over inconsistent frame sequences")
print("- alpha-bounds fitting keeps the full subject inside the desktop window")
print("- broken v0.10 bootstrap removed")
print("- version bumped to 0.9.1 for automatic-update delivery")
