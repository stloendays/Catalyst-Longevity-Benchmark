from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"patch anchor missing: {label}")
    return text.replace(old, new, 1)


hpath = Path("teddy_agent_pet/client/src/PetWindow.h")
h = hpath.read_text(encoding="utf-8")
h = replace_once(h, "#include <QPixmap>\n", "#include <QPixmap>\n#include <QScreen>\n", "QScreen include")
h = replace_once(
    h,
    "private:\n    enum class Action {",
    "private:\n    enum class DockMode { Free, Bottom, Top, Left, Right };\n\n    enum class Action {",
    "DockMode enum",
)
h = replace_once(
    h,
    "    void tickLife();\n    void scheduleBlink();",
    "    void tickLife();\n    void tickDesktop();\n    QScreen *screenForPoint(const QPoint &globalPoint) const;\n    void settleOnDesktop();\n    void ensureOnDesktop();\n    void stepWalkAcrossDesktop();\n    void dockToTaskbar(QScreen *screen=nullptr);\n    void moveToNextScreen();\n    QString dockModeName(DockMode mode) const;\n    DockMode dockModeFromName(const QString &name) const;\n    void scheduleBlink();",
    "desktop methods",
)
h = replace_once(h, "    QTimer hoverTimer_;\n", "    QTimer hoverTimer_;\n    QTimer desktopTimer_;\n", "desktop timer")
h = replace_once(
    h,
    "    bool idleBlinking_{false};\n",
    "    bool idleBlinking_{false};\n    bool followCursor_{true};\n    bool snapToEdges_{true};\n",
    "desktop bools",
)
h = replace_once(
    h,
    "    int lifeSaveTicks_{0};\n    QString answer_;",
    "    int lifeSaveTicks_{0};\n    int cursorStillTicks_{0};\n    DockMode dockMode_{DockMode::Free};\n    QString dockScreenName_;\n    QPoint lastCursorGlobal_;\n    QString answer_;",
    "desktop state",
)
hpath.write_text(h, encoding="utf-8")

cpppath = Path("teddy_agent_pet/client/src/PetWindowV7.cpp")
cpp = cpppath.read_text(encoding="utf-8")
cpp = replace_once(cpp, "#include <QtMath>\n", "#include <QtMath>\n#include <limits>\n", "limits include")
cpp = replace_once(
    cpp,
    "    behavior_.restore(initialSettings);\n    lifeClock_.start();",
    "    behavior_.restore(initialSettings);\n    dockMode_=dockModeFromName(initialSettings.value(\"pet/dock_mode\",\"free\").toString());\n    dockScreenName_=initialSettings.value(\"pet/dock_screen\",\"\").toString();\n    followCursor_=initialSettings.value(\"desktop/follow_cursor\",true).toBool();\n    snapToEdges_=initialSettings.value(\"desktop/snap_to_edges\",true).toBool();\n    lastCursorGlobal_=QCursor::pos();\n    ensureOnDesktop();\n    lifeClock_.start();",
    "restore desktop state",
)
cpp = replace_once(
    cpp,
    "    lifeTimer_.start();\n\n    connect(&composer_",
    "    lifeTimer_.start();\n\n    desktopTimer_.setInterval(1000);\n    connect(&desktopTimer_, &QTimer::timeout, this, &PetWindow::tickDesktop);\n    desktopTimer_.start();\n\n    connect(&composer_",
    "desktop timer setup",
)
cpp = replace_once(
    cpp,
    "PetWindow::~PetWindow() {\n    QSettings lifeSettings;\n    behavior_.save(lifeSettings);",
    "PetWindow::~PetWindow() {\n    QSettings lifeSettings;\n    behavior_.save(lifeSettings);\n    savePosition();",
    "save desktop state",
)
cpp = replace_once(
    cpp,
    "    // Tony notices the cursor even while idle. With no dedicated eye sprite yet,",
    """    // The desktop surface affects Tony's resting pose. Bottom means he is sitting
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

    // Tony notices the cursor even while idle. With no dedicated eye sprite yet,""",
    "dock pose",
)
old_walk = """    if(action_==Action::Walk && !dragging_){
        auto screen=QGuiApplication::screenAt(frameGeometry().center());
        if(!screen) screen=QGuiApplication::primaryScreen();
        const auto area=screen->availableGeometry();
        // Slow desktop walk: one pixel per animation tick rather than gliding constantly.
        QPoint n=pos()+QPoint(1*walkDirection_,0);
        if(n.x()+width()>area.right()) { walkDirection_=-1; n.setX(area.right()-width()); }
        else if(n.x()<area.left()) { walkDirection_=1; n.setX(area.left()); }
        move(n);
    }"""
cpp = replace_once(
    cpp,
    old_walk,
    """    if(action_==Action::Walk && !dragging_){
        stepWalkAcrossDesktop();
    }""",
    "cross-screen walk",
)

desktop_impl = r'''
void PetWindow::tickDesktop(){
    if(dragging_ || mouseDown_) return;

    ensureOnDesktop();

    const QPoint cursor=QCursor::pos();
    if((cursor-lastCursorGlobal_).manhattanLength()<4) ++cursorStillTicks_;
    else cursorStillTicks_=0;
    lastCursorGlobal_=cursor;

    if(!followCursor_ || agentState_!="idle" || composer_.isVisible() ||
       action_!=Action::Idle || actionTimer_.isActive()) return;

    const auto life=behavior_.snapshot();
    const QPoint center=frameGeometry().center();
    const int dx=cursor.x()-center.x();
    const int dy=cursor.y()-center.y();
    const int distance=qAbs(dx)+qAbs(dy);
    const bool recentlyTouched=activityClock_.isValid() && activityClock_.elapsed()<2200;

    // Cursor following is intentionally low-frequency. Tony only takes a short
    // curious walk when the pointer has been resting nearby for a few seconds.
    if(cursorStillTicks_>=3 && !recentlyTouched && distance>150 && distance<430 &&
       life.curiosity>=58 && life.energy>=36 && QRandomGenerator::global()->bounded(100)<32) {
        walkDirection_=(dx>=0) ? 1 : -1;
        if(dockMode_==DockMode::Top || dockMode_==DockMode::Left || dockMode_==DockMode::Right)
            dockMode_=DockMode::Free;
        emotion_="curious";
        setAction(Action::Walk,qBound(650,distance*3,1500));
        cursorStillTicks_=0;
    }
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

    const QRect area=screen->availableGeometry();
    QPoint next=pos()+QPoint(walkDirection_,0);
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

'''
cpp = replace_once(cpp, "void PetWindow::markInteraction(){", desktop_impl + "void PetWindow::markInteraction(){", "desktop implementation")
cpp = replace_once(
    cpp,
    "          dragging_=true;\n          actionTimer_.stop();",
    "          dragging_=true;\n          dockMode_=DockMode::Free;\n          dockScreenName_.clear();\n          actionTimer_.stop();",
    "detach on drag",
)
cpp = replace_once(
    cpp,
    "        behavior_.onDragged(rough);\n        dragging_=false;\n        savePosition();",
    "        behavior_.onDragged(rough);\n        dragging_=false;\n        settleOnDesktop();\n        savePosition();",
    "settle after drag",
)
cpp = replace_once(
    cpp,
    "    auto localSsh=connectionMenu->addAction(uiText(\"Use local SSH tunnel (advanced)\",\"使用本机 SSH 隧道（高级）\"));\n\n    auto *actions=",
    """    auto localSsh=connectionMenu->addAction(uiText("Use local SSH tunnel (advanced)","使用本机 SSH 隧道（高级）"));

    auto *desktopMenu=settings->addMenu(uiText("Desktop behavior","桌面行为"));
    auto followCursor=desktopMenu->addAction(uiText("Gently follow a nearby cursor","轻轻跟随附近的鼠标"));
    followCursor->setCheckable(true);
    followCursor->setChecked(followCursor_);
    auto snapEdges=desktopMenu->addAction(uiText("Snap to screen edges / taskbar","靠近屏幕边缘 / 任务栏时停靠"));
    snapEdges->setCheckable(true);
    snapEdges->setChecked(snapToEdges_);
    desktopMenu->addSeparator();
    auto taskbarHome=desktopMenu->addAction(uiText("Sit by the taskbar","回到任务栏旁边"));
    auto nextDisplay=desktopMenu->addAction(uiText("Move to next display","去下一个显示器"));

    auto *actions=""",
    "desktop menu",
)
cpp = replace_once(
    cpp,
    "    else if(chosen==localSsh) useLocalSshConnection();\n    else if(chosen==idle)",
    """    else if(chosen==localSsh) useLocalSshConnection();
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
    else if(chosen==taskbarHome) dockToTaskbar();
    else if(chosen==nextDisplay) moveToNextScreen();
    else if(chosen==idle)""",
    "desktop menu handlers",
)
cpp = replace_once(
    cpp,
    "    else if(chosen==walk) setAction(Action::Walk,3000);",
    "    else if(chosen==walk) { if(dockMode_==DockMode::Left || dockMode_==DockMode::Right || dockMode_==DockMode::Top) dockMode_=DockMode::Free; setAction(Action::Walk,3000); }",
    "manual walk detach",
)
cpp = replace_once(
    cpp,
    """void PetWindow::restorePosition(){
    QSettings s; auto v=s.value("pet/position");
    if(v.isValid()) move(v.toPoint());
    else { auto a=QGuiApplication::primaryScreen()->availableGeometry(); move(a.right()-width()-40,a.bottom()-height()-20); }
}
void PetWindow::savePosition(){ QSettings().setValue("pet/position",pos()); }""",
    """void PetWindow::restorePosition(){
    QSettings s; auto v=s.value("pet/position");
    if(v.isValid()) move(v.toPoint());
    else { auto a=QGuiApplication::primaryScreen()->availableGeometry(); move(a.right()-width()-40,a.bottom()-height()+1); }
}
void PetWindow::savePosition(){
    QSettings s;
    s.setValue("pet/position",pos());
    s.setValue("pet/dock_mode",dockModeName(dockMode_));
    s.setValue("pet/dock_screen",dockScreenName_);
}""",
    "persistent docking",
)
cpppath.write_text(cpp, encoding="utf-8")

cmake = Path("teddy_agent_pet/client/CMakeLists.txt")
text = cmake.read_text(encoding="utf-8")
text = replace_once(text, "project(TonyDesktopPet VERSION 0.8.0 LANGUAGES CXX)", "project(TonyDesktopPet VERSION 0.8.1 LANGUAGES CXX)", "version")
cmake.write_text(text, encoding="utf-8")

doc = Path("teddy_agent_pet/docs/TONY_DESKTOP_BEHAVIOR.md")
doc.write_text(
    """# Tony Desktop Behavior V0.8.1

Tony is a Teddy dog / toy poodle desktop companion, not a bear. The desktop itself is part of his environment.

## Implemented behavior

- Drag-release settling against the active screen work area.
- Optional snap to bottom/top/left/right screen surfaces. The bottom surface naturally sits above a bottom taskbar.
- Taskbar-aware home action: infer the taskbar side from the difference between `QScreen::geometry()` and `availableGeometry()`.
- Side-edge resting pose: Tony leans inward rather than behaving like a floating window.
- Multi-monitor-safe walking with direct crossing to adjacent displays when their vertical spans overlap.
- Manual “Move to next display” action.
- Periodic visibility recovery after monitor removal, resolution changes, RDP sessions, or docking/undocking a laptop.
- Low-frequency cursor curiosity: Tony may walk a short distance toward a pointer that has been resting nearby, gated by energy and curiosity.
- User controls for cursor following and edge/taskbar snapping.
- Dock mode and display name persist across restarts.

## Safety / usability constraints

Desktop movement never runs while Tony is being dragged, while the chat composer is open, or while the Agent is busy. Cursor-follow behavior is deliberately probabilistic and short so Tony does not constantly chase the pointer.
""",
    encoding="utf-8",
)
