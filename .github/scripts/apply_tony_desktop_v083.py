from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "teddy_agent_pet/client/src/PetWindow.h"
CPP = ROOT / "teddy_agent_pet/client/src/PetWindowV7.cpp"
CMAKE = ROOT / "teddy_agent_pet/client/CMakeLists.txt"
README = ROOT / "teddy_agent_pet/client/README.md"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def replace_block(text: str, start: str, end: str, new_block: str, label: str) -> str:
    i = text.find(start)
    if i < 0:
        raise RuntimeError(f"{label}: start marker not found")
    j = text.find(end, i)
    if j < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:i] + new_block + "\n\n" + text[j:]


h = HEADER.read_text(encoding="utf-8")
h = replace_once(h,
    "        Idle, Curious, Pet, Carried, Land, Dizzy, Stretch, Yawn,\n",
    "        Idle, Curious, Peek, Pet, Carried, Land, Dizzy, Stretch, Yawn,\n",
    "header action enum")
h = replace_once(h,
    "    void tickDesktop();\n",
    "    void tickDesktop();\n    void tickPhysics();\n",
    "header tickPhysics")
h = replace_once(h,
    "    void perchOnForegroundWindow();\n    void startCursorWalk(const QPoint &cursor);\n",
    "    void perchOnForegroundWindow();\n    void walkAlongForegroundWindow();\n    bool wouldHitForegroundWindow(const QRect &nextFrame) const;\n    void startCursorWalk(const QPoint &cursor);\n    void startFall(bool rough);\n    void moveToRestCorner();\n",
    "header desktop helpers")
h = replace_once(h,
    "    QTimer desktopTimer_;\n",
    "    QTimer desktopTimer_;\n    QTimer physicsTimer_;\n",
    "header physics timer")
h = replace_once(h,
    "    bool hiddenForFullscreen_{false};\n    bool hasWalkTarget_{false};\n",
    "    bool hiddenForFullscreen_{false};\n    bool hasWalkTarget_{false};\n    bool gravityEnabled_{true};\n    bool fastCursorChase_{true};\n    bool autoRest_{true};\n    bool edgePeek_{true};\n    bool falling_{false};\n    bool pendingDizzyAfterFall_{false};\n    bool autoRested_{false};\n    bool walkingOnWindow_{false};\n",
    "header behavior flags")
h = replace_once(h,
    "    int cursorStillTicks_{0};\n",
    "    int cursorStillTicks_{0};\n    int fallVelocity_{0};\n    int fallTargetY_{0};\n    int bounceCount_{0};\n",
    "header physics fields")
h = replace_once(h,
    "    QPoint walkTarget_;\n",
    "    QPoint walkTarget_;\n    QRect windowWalkArea_;\n",
    "header window walk area")
HEADER.write_text(h, encoding="utf-8")

cpp = CPP.read_text(encoding="utf-8")
cpp = replace_once(cpp,
    "    hideForFullscreen_=initialSettings.value(\"desktop/hide_for_fullscreen\",true).toBool();\n    perchOnActiveWindow_=initialSettings.value(\"desktop/perch_on_active_window\",false).toBool();\n",
    "    hideForFullscreen_=initialSettings.value(\"desktop/hide_for_fullscreen\",true).toBool();\n    perchOnActiveWindow_=initialSettings.value(\"desktop/perch_on_active_window\",false).toBool();\n    gravityEnabled_=initialSettings.value(\"desktop/gravity\",true).toBool();\n    fastCursorChase_=initialSettings.value(\"desktop/fast_cursor_chase\",true).toBool();\n    autoRest_=initialSettings.value(\"desktop/auto_rest\",true).toBool();\n    edgePeek_=initialSettings.value(\"desktop/edge_peek\",true).toBool();\n",
    "constructor settings")
cpp = replace_once(cpp,
    "    desktopTimer_.setInterval(1000);\n    connect(&desktopTimer_, &QTimer::timeout, this, &PetWindow::tickDesktop);\n    desktopTimer_.start();\n",
    "    // Desktop sensing runs at 4 Hz: quick enough to notice cursor sweeps without\n    // turning foreground-window checks into a busy loop.\n    desktopTimer_.setInterval(250);\n    connect(&desktopTimer_, &QTimer::timeout, this, &PetWindow::tickDesktop);\n    desktopTimer_.start();\n\n    physicsTimer_.setInterval(16);\n    connect(&physicsTimer_, &QTimer::timeout, this, &PetWindow::tickPhysics);\n",
    "constructor timers")
cpp = replace_once(cpp,
    "    case Action::Curious:return \"curious\";\n",
    "    case Action::Curious:return \"curious\";\n    case Action::Peek:return \"idle\";\n",
    "asset key peek")
cpp = replace_once(cpp,
    "    case Action::Curious:return 5;\n",
    "    case Action::Curious:return 5;\n    case Action::Peek:return 5;\n",
    "frame stride peek")
cpp = replace_once(cpp,
    "    case Action::Curious: dy=-1; rotation=1.4*qSin(t*.45); scale=1.008; break;\n",
    "    case Action::Curious: dy=-1; rotation=1.4*qSin(t*.45); scale=1.008; break;\n    case Action::Peek:\n        if(dockMode_==DockMode::Left) { dx=-28; rotation=-4.0; }\n        else if(dockMode_==DockMode::Right) { dx=28; rotation=4.0; }\n        else if(dockMode_==DockMode::Top) { dy=-24; rotation=2.0*qSin(t*.55); }\n        else { dy=-2; scale=1.01; }\n        break;\n",
    "paint peek")
cpp = replace_once(cpp,
    "    if(a!=Action::Walk) hasWalkTarget_=false;\n",
    "    if(a!=Action::Walk) { hasWalkTarget_=false; walkingOnWindow_=false; }\n",
    "setAction walk state")
cpp = replace_once(cpp,
    "    using Impulse=TonyBehaviorEngine::Impulse;\n    const auto impulse=behavior_.chooseIdleImpulse(QTime::currentTime().hour());\n",
    "    if(edgePeek_ && (dockMode_==DockMode::Left || dockMode_==DockMode::Right || dockMode_==DockMode::Top) &&\n       QRandomGenerator::global()->bounded(100)<26) {\n        emotion_=\"curious\";\n        setAction(Action::Peek,1700);\n        scheduleIdleMoment();\n        return;\n    }\n\n    using Impulse=TonyBehaviorEngine::Impulse;\n    const auto impulse=behavior_.chooseIdleImpulse(QTime::currentTime().hour());\n",
    "idle edge peek")
cpp = replace_once(cpp,
    "    case Impulse::Walk:\n        emotion_=\"playful\"; setAction(Action::Walk,3200); break;\n",
    "    case Impulse::Walk:\n        emotion_=\"playful\";\n        if(perchOnActiveWindow_) walkAlongForegroundWindow();\n        else setAction(Action::Walk,3200);\n        break;\n",
    "idle window walk")

new_tick_desktop = '''void PetWindow::tickDesktop(){
    if(dragging_ || mouseDown_ || falling_) return;

    updateForegroundWindowBehavior();
    if(hiddenForFullscreen_) return;
    ensureOnDesktop();

    const QPoint cursor=QCursor::pos();
    const QPoint previousCursor=lastCursorGlobal_;
    const int cursorTravel=(cursor-previousCursor).manhattanLength();
    if(cursorTravel<4) ++cursorStillTicks_;
    else cursorStillTicks_=0;
    lastCursorGlobal_=cursor;

    const bool idleAgent=agentState_=="idle" && !composer_.isVisible();
    const bool actionFree=(action_==Action::Idle || action_==Action::Curious) && !actionTimer_.isActive();

    // After ten quiet minutes Tony chooses the less intrusive lower corner and sleeps.
    if(autoRest_ && !autoRested_ && idleAgent && action_==Action::Idle &&
       activityClock_.isValid() && activityClock_.elapsed()>=10*60*1000) {
        moveToRestCorner();
        return;
    }

    if(perchOnActiveWindow_ && idleAgent && actionFree) {
        perchOnForegroundWindow();
    }

    if(!idleAgent || !actionFree || perchOnActiveWindow_) return;

    const auto life=behavior_.snapshot();
    const QPoint center=frameGeometry().center();
    const int dx=cursor.x()-center.x();
    const int dy=cursor.y()-center.y();
    const int distance=qAbs(dx)+qAbs(dy);
    const bool recentlyTouched=activityClock_.isValid() && activityClock_.elapsed()<2200;

    // A fast pointer sweep near Tony triggers an occasional short chase. Using the
    // swept rectangle catches passes that cross Tony even when both sampled endpoints
    // are already far away.
    QRect cursorSweep(previousCursor,cursor);
    cursorSweep=cursorSweep.normalized().adjusted(-105,-105,105,105);
    const bool sweptNear=cursorSweep.contains(center);
    if(fastCursorChase_ && followCursor_ && !recentlyTouched && cursorTravel>=150 && sweptNear &&
       life.curiosity>=52 && life.energy>=34 && QRandomGenerator::global()->bounded(100)<22) {
        startCursorWalk(cursor);
        cursorStillTicks_=0;
        return;
    }

    if(!followCursor_) return;

    // Four-Hz sensing means 12 still samples is roughly three seconds.
    if(cursorStillTicks_>=12 && !recentlyTouched && distance>150 && distance<520 &&
       life.curiosity>=58 && life.energy>=36 && QRandomGenerator::global()->bounded(100)<14) {
        startCursorWalk(cursor);
        cursorStillTicks_=0;
    }
}'''
cpp = replace_block(cpp, "void PetWindow::tickDesktop(){", "bool PetWindow::foregroundWindowInfo", new_tick_desktop, "tickDesktop block")

insert_before_cursor_walk = '''void PetWindow::walkAlongForegroundWindow(){
    QRect windowRect;
    QScreen *screen=nullptr;
    bool fullscreen=false;
    if(!foregroundWindowInfo(&windowRect,&screen,&fullscreen) || !screen || fullscreen) {
        emotion_="curious";
        setAction(Action::Curious,900);
        return;
    }

    const QRect area=screen->availableGeometry();
    const QRect visibleWindow=windowRect.intersected(area);
    if(visibleWindow.width()<width()+80 || visibleWindow.height()<120) {
        emotion_="curious";
        setAction(Action::Curious,900);
        return;
    }

    const int shadowY=196;
    const int maxX=qMax(area.left(),area.right()-width()+1);
    const int topPerch=visibleWindow.top()-shadowY;
    const int bottomPerch=visibleWindow.bottom()-shadowY;
    const int y=topPerch>=area.top() ? qMin(topPerch,area.bottom()-height()+1)
                                     : qBound(area.top(),bottomPerch,area.bottom()-height()+1);
    const int leftX=qBound(area.left(),visibleWindow.left()+12,maxX);
    const int rightX=qBound(area.left(),visibleWindow.right()-width()+1-12,maxX);
    if(rightX-leftX<70) return;

    // Start from the nearest valid point on the same ledge, then head toward the
    // opposite half of the active window. This makes the movement read as walking
    // along a surface rather than teleporting between arbitrary desktop points.
    QPoint start=qBound(leftX,pos().x(),rightX)==pos().x() ? QPoint(pos().x(),y)
                                                          : QPoint(qBound(leftX,pos().x(),rightX),y);
    move(start);
    const int midpoint=(leftX+rightX)/2;
    const int targetX=start.x()<=midpoint ? rightX : leftX;
    windowWalkArea_=QRect(leftX,y,rightX-leftX+1,1);
    walkTarget_=QPoint(targetX,y);
    hasWalkTarget_=true;
    walkingOnWindow_=true;
    walkDirection_=targetX>=start.x() ? 1 : -1;
    emotion_="playful";
    setAction(Action::Walk,qBound(1800,qAbs(targetX-start.x())*12,7000));
    hasWalkTarget_=true;
    walkingOnWindow_=true;
    walkTarget_=QPoint(targetX,y);
}

bool PetWindow::wouldHitForegroundWindow(const QRect &nextFrame) const {
    if(walkingOnWindow_) return false;
    QRect obstacle;
    QScreen *screen=nullptr;
    bool fullscreen=false;
    if(!foregroundWindowInfo(&obstacle,&screen,&fullscreen) || fullscreen) return false;
    obstacle=obstacle.adjusted(-6,-6,6,6);
    const QRect currentBody=frameGeometry().adjusted(32,30,-32,-22);
    const QRect nextBody=nextFrame.adjusted(32,30,-32,-22);
    // If Tony already overlaps the active app, do not trap him there. Collision
    // avoidance only applies when a walk would newly enter the window.
    return !currentBody.intersects(obstacle) && nextBody.intersects(obstacle);
}

'''
cpp = replace_once(cpp,
    "void PetWindow::startCursorWalk(const QPoint &cursor){\n",
    insert_before_cursor_walk + "void PetWindow::startCursorWalk(const QPoint &cursor){\n",
    "insert window walking helpers")

physics_methods = '''void PetWindow::startFall(bool rough){
    QScreen *screen=screenForPoint(frameGeometry().center());
    if(!screen) {
        settleOnDesktop();
        savePosition();
        emotion_=rough ? "dizzy" : "playful";
        setAction(rough ? Action::Dizzy : Action::Land, rough ? 1500 : 650);
        return;
    }

    const QRect area=screen->availableGeometry();
    QPoint p=pos();
    p.setX(qBound(area.left(),p.x(),qMax(area.left(),area.right()-width()+1)));
    p.setY(qMin(p.y(),area.bottom()-height()+1));
    move(p);

    dockMode_=DockMode::Free;
    dockScreenName_=screen->name();
    hasWalkTarget_=false;
    walkingOnWindow_=false;
    falling_=true;
    pendingDizzyAfterFall_=rough;
    fallVelocity_=0;
    bounceCount_=0;
    fallTargetY_=area.bottom()-height()+1;
    actionTimer_.stop();
    action_=Action::Carried;
    frame_=0;
    physicsTimer_.start();
    update();
}

void PetWindow::tickPhysics(){
    if(!falling_) {
        physicsTimer_.stop();
        return;
    }

    QScreen *screen=screenForPoint(frameGeometry().center());
    if(!screen) return;
    const QRect area=screen->availableGeometry();
    fallTargetY_=area.bottom()-height()+1;

    fallVelocity_ += 2;
    int nextY=pos().y()+fallVelocity_;
    if(nextY>=fallTargetY_) {
        move(pos().x(),fallTargetY_);
        if(bounceCount_<2 && qAbs(fallVelocity_)>=7) {
            ++bounceCount_;
            fallVelocity_=-qMax(3,qAbs(fallVelocity_)*35/100);
            action_=Action::Land;
            frame_=0;
            update();
            return;
        }

        falling_=false;
        physicsTimer_.stop();
        dockMode_=DockMode::Bottom;
        dockScreenName_=screen->name();
        savePosition();
        const bool dizzy=pendingDizzyAfterFall_;
        pendingDizzyAfterFall_=false;
        if(dizzy) {
            emotion_="dizzy";
            setAction(Action::Dizzy,1500);
            showBubble(uiText("Fast trip. My curls are still catching up.","飞得有点快，我的卷毛还没反应过来。"),3200);
        } else {
            emotion_="playful";
            setAction(Action::Land,700);
        }
        return;
    }

    move(pos().x(),nextY);
    const QPoint anchor=mapToGlobal(QPoint(width()/2,20));
    bubble_.follow(anchor);
    composer_.follow(anchor);
}

void PetWindow::moveToRestCorner(){
    QScreen *screen=screenForPoint(frameGeometry().center());
    if(!screen) screen=QGuiApplication::primaryScreen();
    if(!screen) return;

    const QRect area=screen->availableGeometry();
    const int leftX=area.left()+8;
    const int rightX=qMax(leftX,area.right()-width()+1-8);
    const int y=area.bottom()-height()+1;
    const QPoint cursor=QCursor::pos();
    const int leftDistance=qAbs(cursor.x()-(leftX+width()/2));
    const int rightDistance=qAbs(cursor.x()-(rightX+width()/2));
    const int x=leftDistance>rightDistance ? leftX : rightX;

    perchOnActiveWindow_=false;
    QSettings().setValue("desktop/perch_on_active_window",false);
    dockMode_=DockMode::Bottom;
    dockScreenName_=screen->name();
    move(x,y);
    savePosition();
    autoRested_=true;
    emotion_="sleepy";
    setAction(Action::Sleep,0);
    tray_.setToolTip("Tony · sleeping in a quiet corner");
}

'''
cpp = replace_once(cpp,
    "QScreen *PetWindow::screenForPoint(const QPoint &globalPoint) const {\n",
    physics_methods + "QScreen *PetWindow::screenForPoint(const QPoint &globalPoint) const {\n",
    "insert physics and rest")

new_step_walk = '''void PetWindow::stepWalkAcrossDesktop(){
    QScreen *screen=QGuiApplication::screenAt(frameGeometry().center());
    if(!screen) screen=screenForPoint(frameGeometry().center());
    if(!screen) return;

    if(walkingOnWindow_) {
        const int remaining=walkTarget_.x()-pos().x();
        if(qAbs(remaining)<=4) {
            move(walkTarget_);
            hasWalkTarget_=false;
            walkingOnWindow_=false;
            actionTimer_.stop();
            emotion_="content";
            setAction(Action::Curious,900);
            return;
        }
        walkDirection_=remaining>0 ? 1 : -1;
        const int step=qMin(4,qAbs(remaining));
        QPoint next=pos()+QPoint(walkDirection_*step,0);
        next.setY(windowWalkArea_.top());
        next.setX(qBound(windowWalkArea_.left(),next.x(),windowWalkArea_.right()));
        move(next);
        dockScreenName_=screen->name();
        return;
    }

    if(hasWalkTarget_) {
        const int remaining=walkTarget_.x()-pos().x();
        if(qAbs(remaining)<=5) {
            move(walkTarget_);
            if(auto *arrived=screenForPoint(frameGeometry().center())) dockScreenName_=arrived->name();
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
    const QRect nextFrame(next,QSize(width(),height()));

    if(wouldHitForegroundWindow(nextFrame)) {
        if(hasWalkTarget_) {
            hasWalkTarget_=false;
            actionTimer_.stop();
            emotion_="curious";
            setAction(Action::Curious,900);
        } else {
            walkDirection_=-walkDirection_;
            emotion_="curious";
        }
        return;
    }

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
}'''
cpp = replace_block(cpp, "void PetWindow::stepWalkAcrossDesktop(){", "void PetWindow::dockToTaskbar", new_step_walk, "stepWalk block")

cpp = replace_once(cpp,
    "void PetWindow::markInteraction(){\n    if(activityClock_.isValid()) activityClock_.restart();\n    else activityClock_.start();\n}\n",
    "void PetWindow::markInteraction(){\n    if(activityClock_.isValid()) activityClock_.restart();\n    else activityClock_.start();\n    if(autoRested_) {\n        autoRested_=false;\n        tray_.setToolTip(agent_.connected() ? \"Tony · connected\" : \"Tony · waiting for server\");\n        if(action_==Action::Sleep && agentState_==\"idle\") {\n            emotion_=\"sleepy\";\n            setAction(Action::Yawn,1100);\n        }\n    }\n}\n",
    "markInteraction wake")
cpp = replace_once(cpp,
    "    case Action::Idle:return \"idle\"; case Action::Curious:return \"curious\"; case Action::Pet:return \"being petted\";\n",
    "    case Action::Idle:return \"idle\"; case Action::Curious:return \"curious\"; case Action::Peek:return \"peeking from the edge\"; case Action::Pet:return \"being petted\";\n",
    "action name peek")
cpp = replace_once(cpp,
    "void PetWindow::mouseReleaseEvent(QMouseEvent *e){\n    if(e->button()!=Qt::LeftButton || !mouseDown_) return;\n    mouseDown_=false;\n    if(dragging_) {\n        const bool rough=dragTravel_>850 || (pressClock_.isValid() && pressClock_.elapsed()<450 && dragTravel_>360);\n        behavior_.onDragged(rough);\n        dragging_=false;\n        settleOnDesktop();\n        savePosition();\n        if(rough) {\n  emotion_=\"dizzy\";\n  setAction(Action::Dizzy,1500);\n  showBubble(uiText(\"Fast trip. My curls are still catching up.\",\"飞得有点快，我的卷毛还没反应过来。\"),3200);\n        } else {\n  emotion_=\"playful\";\n  setAction(Action::Land,650);\n        }\n    } else {\n        handleTap(e->position().toPoint());\n    }\n}\n",
    "void PetWindow::mouseReleaseEvent(QMouseEvent *e){\n    if(e->button()!=Qt::LeftButton || !mouseDown_) return;\n    mouseDown_=false;\n    if(dragging_) {\n        const bool rough=dragTravel_>850 || (pressClock_.isValid() && pressClock_.elapsed()<450 && dragTravel_>360);\n        behavior_.onDragged(rough);\n        dragging_=false;\n        if(gravityEnabled_) {\n            startFall(rough);\n        } else {\n            settleOnDesktop();\n            savePosition();\n            if(rough) {\n                emotion_=\"dizzy\";\n                setAction(Action::Dizzy,1500);\n                showBubble(uiText(\"Fast trip. My curls are still catching up.\",\"飞得有点快，我的卷毛还没反应过来。\"),3200);\n            } else {\n                emotion_=\"playful\";\n                setAction(Action::Land,650);\n            }\n        }\n    } else {\n        handleTap(e->position().toPoint());\n    }\n}\n",
    "mouse release gravity")
cpp = replace_once(cpp,
    "void PetWindow::contextMenuEvent(QContextMenuEvent *e){\n    QMenu m;\n",
    "void PetWindow::contextMenuEvent(QContextMenuEvent *e){\n    markInteraction();\n    QMenu m;\n",
    "context menu wake")
cpp = replace_once(cpp,
    "    auto perchWindow=desktopMenu->addAction(uiText(\"Perch on the active window edge\",\"坐在当前窗口边缘\"));\n    perchWindow->setCheckable(true);\n    perchWindow->setChecked(perchOnActiveWindow_);\n    desktopMenu->addSeparator();\n    auto walkToCursor=desktopMenu->addAction(uiText(\"Walk to the cursor now\",\"现在走到鼠标旁边\"));\n",
    "    auto perchWindow=desktopMenu->addAction(uiText(\"Perch on the active window edge\",\"坐在当前窗口边缘\"));\n    perchWindow->setCheckable(true);\n    perchWindow->setChecked(perchOnActiveWindow_);\n    auto gravity=desktopMenu->addAction(uiText(\"Gravity and bounce after dragging\",\"拖起来后有重力下落和弹跳\"));\n    gravity->setCheckable(true);\n    gravity->setChecked(gravityEnabled_);\n    auto fastChase=desktopMenu->addAction(uiText(\"React to fast cursor sweeps\",\"鼠标快速掠过时会追一下\"));\n    fastChase->setCheckable(true);\n    fastChase->setChecked(fastCursorChase_);\n    auto edgePeek=desktopMenu->addAction(uiText(\"Peek from screen edges\",\"停在屏幕边缘时会探头\"));\n    edgePeek->setCheckable(true);\n    edgePeek->setChecked(edgePeek_);\n    auto autoRest=desktopMenu->addAction(uiText(\"Sleep in a corner after 10 quiet minutes\",\"10 分钟无人操作后去角落睡觉\"));\n    autoRest->setCheckable(true);\n    autoRest->setChecked(autoRest_);\n    desktopMenu->addSeparator();\n    auto walkWindow=desktopMenu->addAction(uiText(\"Walk along the active window\",\"沿当前窗口边缘走一走\"));\n    auto walkToCursor=desktopMenu->addAction(uiText(\"Walk to the cursor now\",\"现在走到鼠标旁边\"));\n",
    "desktop menu options")
cpp = replace_once(cpp,
    "    else if(chosen==perchWindow) {\n        perchOnActiveWindow_=perchWindow->isChecked();\n        QSettings().setValue(\"desktop/perch_on_active_window\",perchOnActiveWindow_);\n        if(perchOnActiveWindow_) { hasWalkTarget_=false; perchOnForegroundWindow(); }\n        else savePosition();\n    }\n    else if(chosen==walkToCursor) { perchOnActiveWindow_=false; QSettings().setValue(\"desktop/perch_on_active_window\",false); startCursorWalk(QCursor::pos()); }\n",
    "    else if(chosen==perchWindow) {\n        perchOnActiveWindow_=perchWindow->isChecked();\n        QSettings().setValue(\"desktop/perch_on_active_window\",perchOnActiveWindow_);\n        if(perchOnActiveWindow_) { hasWalkTarget_=false; perchOnForegroundWindow(); }\n        else savePosition();\n    }\n    else if(chosen==gravity) {\n        gravityEnabled_=gravity->isChecked();\n        QSettings().setValue(\"desktop/gravity\",gravityEnabled_);\n    }\n    else if(chosen==fastChase) {\n        fastCursorChase_=fastChase->isChecked();\n        QSettings().setValue(\"desktop/fast_cursor_chase\",fastCursorChase_);\n    }\n    else if(chosen==edgePeek) {\n        edgePeek_=edgePeek->isChecked();\n        QSettings().setValue(\"desktop/edge_peek\",edgePeek_);\n    }\n    else if(chosen==autoRest) {\n        autoRest_=autoRest->isChecked();\n        QSettings().setValue(\"desktop/auto_rest\",autoRest_);\n        if(!autoRest_) autoRested_=false;\n    }\n    else if(chosen==walkWindow) walkAlongForegroundWindow();\n    else if(chosen==walkToCursor) { perchOnActiveWindow_=false; QSettings().setValue(\"desktop/perch_on_active_window\",false); startCursorWalk(QCursor::pos()); }\n",
    "desktop menu handlers")
cpp = replace_once(cpp,
    "    else if(chosen==walk) { if(dockMode_==DockMode::Left || dockMode_==DockMode::Right || dockMode_==DockMode::Top) dockMode_=DockMode::Free; setAction(Action::Walk,3000); }\n",
    "    else if(chosen==walk) {\n        if(perchOnActiveWindow_) walkAlongForegroundWindow();\n        else { if(dockMode_==DockMode::Left || dockMode_==DockMode::Right || dockMode_==DockMode::Top) dockMode_=DockMode::Free; setAction(Action::Walk,3000); }\n    }\n",
    "manual walk on window")
CPP.write_text(cpp, encoding="utf-8")

cm = CMAKE.read_text(encoding="utf-8")
cm = replace_once(cm, "project(TonyDesktopPet VERSION 0.8.2 LANGUAGES CXX)",
                  "project(TonyDesktopPet VERSION 0.8.3 LANGUAGES CXX)", "cmake version")
CMAKE.write_text(cm, encoding="utf-8")

readme = README.read_text(encoding="utf-8")
section = '''\n\n## V0.8.3 desktop-world behavior\n\nTony now treats the Windows desktop as a physical environment rather than a flat overlay:\n\n- walks along the active window edge when asked (and during some idle walks while perched);\n- avoids newly entering the foreground window while wandering, turning back or stopping to inspect the obstacle;\n- occasionally chases a fast cursor sweep that passes nearby;\n- peeks from left/right/top screen edges without requiring dedicated artwork;\n- falls under gravity and bounces after being dragged, with a switch to disable the physics;\n- after ten quiet minutes, moves to the less intrusive bottom corner and sleeps until the next interaction.\n\nAll of these behaviors are optional from **Settings → Desktop behavior**.\n'''
if "## V0.8.3 desktop-world behavior" not in readme:
    readme += section
README.write_text(readme, encoding="utf-8")

print("Tony V0.8.3 desktop behavior patch applied")
