#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "bear_agent_pet/client/src/main.cpp"
PET = ROOT / "bear_agent_pet/client/src/PetWindowV7.cpp"
CMAKE = ROOT / "bear_agent_pet/client/CMakeLists.txt"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"Missing patch anchor: {label}")
    return text.replace(old, new, 1)


main = MAIN.read_text(encoding="utf-8")
main = main.replace("#include <QSettings>\n", "", 1)
main = main.replace("#include <QToolButton>\n", "", 1)

settings_button = '''    // A small, quiet settings affordance on the pet itself. It avoids another
    // tray icon and keeps the settings page discoverable without changing Tony's calm idle behavior.
    QToolButton settingsButton(&pet);
    settingsButton.setText(QStringLiteral("⚙"));
    settingsButton.setToolTip(QStringLiteral("Settings"));
    settingsButton.setFixedSize(26, 26);
    settingsButton.move(pet.width() - settingsButton.width() - 5, 5);
    settingsButton.setCursor(Qt::PointingHandCursor);
    settingsButton.setStyleSheet(QStringLiteral(
        "QToolButton { background: rgba(20,20,20,95); color: white; border: 0; border-radius: 13px; font-size: 14px; }"
        "QToolButton:hover { background: rgba(20,20,20,150); }"));
    settingsButton.show();

'''
main = replace_once(main, settings_button, "", "floating settings gear")
main = main.replace(
    "    QObject::connect(&settingsButton, &QToolButton::clicked, &app, showSettings);\n\n",
    "",
    1,
)

auto_pair = '''    // Trusted-friend flow:
    // - first launch: automatically ask only for the reusable friend code;
    // - after pairing: the per-device token is protected by Windows DPAPI;
    // - later launches: PetWindow connects automatically and AgentClient reconnects on outages.
    QSettings settings;
    if(settings.value("agent/token").toString().trimmed().isEmpty()) {
        QTimer::singleShot(700, &pet, &PetWindow::configureConnection);
    }

'''
replacement_pair = '''    // First launch stays non-modal. PetWindow already shows a short connection
    // hint; the friend-code dialog opens only when the user explicitly chooses
    // Connect to Tony. This keeps the pet visible instead of covering it at startup.

'''
main = replace_once(main, auto_pair, replacement_pair, "automatic first-run pairing dialog")
MAIN.write_text(main, encoding="utf-8")

pet = PET.read_text(encoding="utf-8")
old_draw = '''    const QPointF center(width()/2.0+dx,108+dy);
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

new_draw = '''    p.save();
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
'''
pet = replace_once(pet, old_draw, new_draw, "ground-line renderer")
PET.write_text(pet, encoding="utf-8")

cmake = CMAKE.read_text(encoding="utf-8")
if "project(TonyDesktopPet VERSION 0.9.2 LANGUAGES CXX)" in cmake:
    cmake = cmake.replace(
        "project(TonyDesktopPet VERSION 0.9.2 LANGUAGES CXX)",
        "project(TonyDesktopPet VERSION 0.9.3 LANGUAGES CXX)",
        1,
    )
elif "project(TonyDesktopPet VERSION 0.9.3 LANGUAGES CXX)" not in cmake:
    raise SystemExit("Unexpected Tony version; refusing unsafe rewrite")
CMAKE.write_text(cmake, encoding="utf-8")

print("TONY_RUNTIME_APPEARANCE_FIX=PASS")
print("- no automatic pairing dialog over Tony")
print("- floating settings gear removed")
print("- all poses share the 205 px ground line")
print("- leftward walking mirrors the walk art")
print("- hug keeps a little more of the native zoom")
print("- version bumped to 0.9.3")
