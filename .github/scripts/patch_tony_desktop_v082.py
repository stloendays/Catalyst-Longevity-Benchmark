from pathlib import Path

ROOT = Path("teddy_agent_pet/client")
CPP = ROOT / "src/PetWindowV7.cpp"
HDR = ROOT / "src/PetWindow.h"
CMAKE = ROOT / "CMakeLists.txt"


def replace_one(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, got {count}")
    return text.replace(old, new, 1)


h = HDR.read_text(encoding="utf-8")
h = replace_one(h, '#include <QPoint>\n', '#include <QPoint>\n#include <QRect>\n', 'header QRect include')
h = replace_one(
    h,
    '    void tickDesktop();\n    QScreen *screenForPoint(const QPoint &globalPoint) const;\n',
    '    void tickDesktop();\n'
    '    void updateForegroundWindowBehavior();\n'
    '    bool foregroundWindowInfo(QRect *rect, QScreen **screen, bool *fullscreen) const;\n'
    '    void perchOnForegroundWindow();\n'
    '    void startCursorWalk(const QPoint &cursor);\n'
    '    QScreen *screenForPoint(const QPoint &globalPoint) const;\n',
    'desktop method declarations')
h = replace_one(
    h,
    '    bool followCursor_{true};\n    bool snapToEdges_{true};\n',
    '    bool followCursor_{true};\n'
    '    bool snapToEdges_{true};\n'
    '    bool hideForFullscreen_{true};\n'
    '    bool perchOnActiveWindow_{false};\n'
    '    bool hiddenForFullscreen_{false};\n'
    '    bool hasWalkTarget_{false};\n',
    'desktop bool members')
h = replace_one(
    h,
    '    QPoint lastCursorGlobal_;\n',
    '    QPoint lastCursorGlobal_;\n    QPoint walkTarget_;\n',
    'walk target member')
HDR.write_text(h, encoding="utf-8")

cpp = CPP.read_text(encoding="utf-8")
cpp = replace_one(
    cpp,
    '    followCursor_=initialSettings.value("desktop/follow_cursor",true).toBool();\n'
    '    snapToEdges_=initialSettings.value("desktop/snap_to_edges",true).toBool();\n',
    '    followCursor_=initialSettings.value("desktop/follow_cursor",true).toBool();\n'
    '    snapToEdges_=initialSettings.value("desktop/snap_to_edges",true).toBool();\n'
    '    hideForFullscreen_=initialSettings.value("desktop/hide_for_fullscreen",true).toBool();\n'
    '    perchOnActiveWindow_=initialSettings.value("desktop/perch_on_active_window",false).toBool();\n',
    'desktop settings restore')
cpp = replace_one(
    cpp,
    'void PetWindow::setAction(Action a,int durationMs){\n'
    '    action_=a; frame_=0; basePos_=pos();\n',
    'void PetWindow::setAction(Action a,int durationMs){\n'
    '    action_=a; frame_=0; basePos_=pos();\n'
    '    if(a!=Action::Walk) hasWalkTarget_=false;\n',
    'setAction clears walk target')

old_tick = '''void PetWindow::tickDesktop(){
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
'''
new_tick = '''void PetWindow::tickDesktop(){
    if(dragging_ || mouseDown_) return;

    updateForegroundWindowBehavior();
    if(hiddenForFullscreen_) return;
    ensureOnDesktop();

    const QPoint cursor=QCursor::pos();
    if((cursor-lastCursorGlobal_).manhattanLength()<4) ++cursorStillTicks_;
    else cursorStillTicks_=0;
    lastCursorGlobal_=cursor;

    if(perchOnActiveWindow_ && agentState_=="idle" && !composer_.isVisible() &&
       (action_==Action::Idle || action_==Action::Curious) && !actionTimer_.isActive()) {
        perchOnForegroundWindow();
    }

    if(!followCursor_ || perchOnActiveWindow_ || agentState_!="idle" || composer_.isVisible() ||
       action_!=Action::Idle || actionTimer_.isActive()) return;

    const auto life=behavior_.snapshot();
    const QPoint center=frameGeometry().center();
    const int dx=cursor.x()-center.x();
    const int dy=cursor.y()-center.y();
    const int distance=qAbs(dx)+qAbs(dy);
    const bool recentlyTouched=activityClock_.isValid() && activityClock_.elapsed()<2200;

    // Cursor following is intentionally low-frequency, but unlike V0.8.1 Tony now
    // stores a real destination and walks toward it instead of taking a token 20px step.
    if(cursorStillTicks_>=3 && !recentlyTouched && distance>150 && distance<520 &&
       life.curiosity>=58 && life.energy>=36 && QRandomGenerator::global()->bounded(100)<32) {
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
    const bool isFullscreen=
        windowRect.left()<=full.left()+tolerance &&
        windowRect.top()<=full.top()+tolerance &&
        windowRect.right()-1>=full.right()-tolerance &&
        windowRect.bottom()-1>=full.bottom()-tolerance;

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
'''
cpp = replace_one(cpp, old_tick, new_tick, 'tickDesktop and foreground behavior')

old_walk = '''void PetWindow::stepWalkAcrossDesktop(){
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
'''
new_walk = '''void PetWindow::stepWalkAcrossDesktop(){
    QScreen *screen=QGuiApplication::screenAt(frameGeometry().center());
    if(!screen) screen=screenForPoint(frameGeometry().center());
    if(!screen) return;

    if(hasWalkTarget_) {
        const int remaining=walkTarget_.x()-pos().x();
        if(qAbs(remaining)<=5) {
            move(walkTarget_);
            dockScreenName_=screenForPoint(frameGeometry().center()) ? screenForPoint(frameGeometry().center())->name() : dockScreenName_;
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
        // There is no reachable display in the requested direction. Stop at the
        // current screen edge instead of oscillating forever.
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
'''
cpp = replace_one(cpp, old_walk, new_walk, 'walk target movement')

cpp = replace_one(
    cpp,
    '          dragging_=true;\n'
    '          dockMode_=DockMode::Free;\n',
    '          dragging_=true;\n'
    '          if(perchOnActiveWindow_) {\n'
    '              perchOnActiveWindow_=false;\n'
    '              QSettings().setValue("desktop/perch_on_active_window",false);\n'
    '          }\n'
    '          hasWalkTarget_=false;\n'
    '          dockMode_=DockMode::Free;\n',
    'manual drag overrides active-window perch')

cpp = replace_one(
    cpp,
    '    auto snapEdges=desktopMenu->addAction(uiText("Snap to screen edges / taskbar","靠近屏幕边缘 / 任务栏时停靠"));\n'
    '    snapEdges->setCheckable(true);\n'
    '    snapEdges->setChecked(snapToEdges_);\n'
    '    desktopMenu->addSeparator();\n'
    '    auto taskbarHome=desktopMenu->addAction(uiText("Sit by the taskbar","回到任务栏旁边"));\n'
    '    auto nextDisplay=desktopMenu->addAction(uiText("Move to next display","去下一个显示器"));\n',
    '    auto snapEdges=desktopMenu->addAction(uiText("Snap to screen edges / taskbar","靠近屏幕边缘 / 任务栏时停靠"));\n'
    '    snapEdges->setCheckable(true);\n'
    '    snapEdges->setChecked(snapToEdges_);\n'
    '    auto fullscreenAware=desktopMenu->addAction(uiText("Hide during fullscreen apps","全屏应用时自动躲起来"));\n'
    '    fullscreenAware->setCheckable(true);\n'
    '    fullscreenAware->setChecked(hideForFullscreen_);\n'
    '    auto perchWindow=desktopMenu->addAction(uiText("Perch on the active window edge","坐在当前窗口边缘"));\n'
    '    perchWindow->setCheckable(true);\n'
    '    perchWindow->setChecked(perchOnActiveWindow_);\n'
    '    desktopMenu->addSeparator();\n'
    '    auto walkToCursor=desktopMenu->addAction(uiText("Walk to the cursor now","现在走到鼠标旁边"));\n'
    '    auto taskbarHome=desktopMenu->addAction(uiText("Sit by the taskbar","回到任务栏旁边"));\n'
    '    auto nextDisplay=desktopMenu->addAction(uiText("Move to next display","去下一个显示器"));\n',
    'desktop menu actions')

cpp = replace_one(
    cpp,
    '    else if(chosen==snapEdges) {\n'
    '        snapToEdges_=snapEdges->isChecked();\n'
    '        QSettings().setValue("desktop/snap_to_edges",snapToEdges_);\n'
    '        if(snapToEdges_) settleOnDesktop();\n'
    '        else { dockMode_=DockMode::Free; savePosition(); }\n'
    '    }\n'
    '    else if(chosen==taskbarHome) dockToTaskbar();\n'
    '    else if(chosen==nextDisplay) moveToNextScreen();\n',
    '    else if(chosen==snapEdges) {\n'
    '        snapToEdges_=snapEdges->isChecked();\n'
    '        QSettings().setValue("desktop/snap_to_edges",snapToEdges_);\n'
    '        if(snapToEdges_) settleOnDesktop();\n'
    '        else { dockMode_=DockMode::Free; savePosition(); }\n'
    '    }\n'
    '    else if(chosen==fullscreenAware) {\n'
    '        hideForFullscreen_=fullscreenAware->isChecked();\n'
    '        QSettings().setValue("desktop/hide_for_fullscreen",hideForFullscreen_);\n'
    '        if(!hideForFullscreen_ && hiddenForFullscreen_) { hiddenForFullscreen_=false; show(); ensureOnDesktop(); }\n'
    '    }\n'
    '    else if(chosen==perchWindow) {\n'
    '        perchOnActiveWindow_=perchWindow->isChecked();\n'
    '        QSettings().setValue("desktop/perch_on_active_window",perchOnActiveWindow_);\n'
    '        if(perchOnActiveWindow_) { hasWalkTarget_=false; perchOnForegroundWindow(); }\n'
    '        else savePosition();\n'
    '    }\n'
    '    else if(chosen==walkToCursor) { perchOnActiveWindow_=false; QSettings().setValue("desktop/perch_on_active_window",false); startCursorWalk(QCursor::pos()); }\n'
    '    else if(chosen==taskbarHome) { perchOnActiveWindow_=false; QSettings().setValue("desktop/perch_on_active_window",false); dockToTaskbar(); }\n'
    '    else if(chosen==nextDisplay) { perchOnActiveWindow_=false; QSettings().setValue("desktop/perch_on_active_window",false); moveToNextScreen(); }\n',
    'desktop menu handlers')

CPP.write_text(cpp, encoding="utf-8")

cmake = CMAKE.read_text(encoding="utf-8")
cmake = replace_one(cmake, 'project(TonyDesktopPet VERSION 0.8.1 LANGUAGES CXX)', 'project(TonyDesktopPet VERSION 0.8.2 LANGUAGES CXX)', 'CMake version')
CMAKE.write_text(cmake, encoding="utf-8")

print('TONY_DESKTOP_V082_PATCH=PASS')
