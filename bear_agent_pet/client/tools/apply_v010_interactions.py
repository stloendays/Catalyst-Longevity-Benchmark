#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if old not in text:
        raise SystemExit(f"Expected patch anchor not found in {path}: {old[:120]!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


header = ROOT / "src" / "PetWindow.h"
cpp = ROOT / "src" / "PetWindowV7.cpp"
cmake = ROOT / "CMakeLists.txt"
validator = ROOT / "tools" / "validate_state_assets.py"

replace_once(
    header,
    "        AdjustGlasses, RemoveGlasses, Wave\n",
    "        AdjustGlasses, RemoveGlasses, Wave,\n"
    "        Pat, Lifted, Landing, Dizzy, Stretch, Yawn, CursorWatch\n",
)
replace_once(
    header,
    "    bool dragging_{false};\n    bool idleBlinking_{false};\n",
    "    bool dragging_{false};\n"
    "    bool movedDuringDrag_{false};\n"
    "    QPoint dragStartGlobal_;\n"
    "    QPoint lastDragGlobal_;\n"
    "    int dragTravel_{0};\n"
    "    bool idleBlinking_{false};\n",
)

replace_once(
    cpp,
    '        "ask_hug","hug","blush","blush_wave","study","adjust_glasses","remove_glasses","wave"\n',
    '        "ask_hug","hug","blush","blush_wave","study","adjust_glasses","remove_glasses","wave",\n'
    '        "pat","lifted","landing","dizzy","stretch","yawn","cursor_watch"\n',
)
replace_once(
    cpp,
    '    case Action::Wave:return "wave";\n',
    '    case Action::Wave:return "wave";\n'
    '    case Action::Pat:return "pat";\n'
    '    case Action::Lifted:return "lifted";\n'
    '    case Action::Landing:return "landing";\n'
    '    case Action::Dizzy:return "dizzy";\n'
    '    case Action::Stretch:return "stretch";\n'
    '    case Action::Yawn:return "yawn";\n'
    '    case Action::CursorWatch:return "cursor_watch";\n',
)
replace_once(
    cpp,
    '    case Action::Blush:return 6;\n    case Action::Idle:return 7;\n',
    '    case Action::Blush:return 6;\n'
    '    case Action::Pat:return 6;\n'
    '    case Action::Lifted:return 6;\n'
    '    case Action::Landing:return 4;\n'
    '    case Action::Dizzy:return 5;\n'
    '    case Action::Stretch:return 7;\n'
    '    case Action::Yawn:return 8;\n'
    '    case Action::CursorWatch:return 6;\n'
    '    case Action::Idle:return 7;\n',
)
replace_once(
    cpp,
    '    case Action::Wave: dy=-qAbs(int(3*qSin(t*1.6))); rotation=3.0*qSin(t*1.8); break;\n',
    '    case Action::Wave: dy=-qAbs(int(3*qSin(t*1.6))); rotation=3.0*qSin(t*1.8); break;\n'
    '    case Action::Pat: break;\n'
    '    case Action::Lifted: dy=-4; break;\n'
    '    case Action::Landing: break;\n'
    '    case Action::Dizzy: break;\n'
    '    case Action::Stretch: break;\n'
    '    case Action::Yawn: break;\n'
    '    case Action::CursorWatch: break;\n',
)
replace_once(
    cpp,
    '    if(n=="blanket" || n=="warm_hands" || n=="tea") return Action::Shiver;\n    return Action::Idle;\n',
    '    if(n=="blanket" || n=="warm_hands" || n=="tea") return Action::Shiver;\n'
    '    if(n=="pat" || n=="head_pat" || n=="pet_head") return Action::Pat;\n'
    '    if(n=="lifted" || n=="picked_up" || n=="pick_up") return Action::Lifted;\n'
    '    if(n=="landing" || n=="land") return Action::Landing;\n'
    '    if(n=="dizzy" || n=="woozy") return Action::Dizzy;\n'
    '    if(n=="stretch" || n=="stretching") return Action::Stretch;\n'
    '    if(n=="yawn" || n=="yawning") return Action::Yawn;\n'
    '    if(n=="cursor_watch" || n=="watch_cursor" || n=="mouse_watch") return Action::CursorWatch;\n'
    '    return Action::Idle;\n',
)
replace_once(
    cpp,
    '    else if(r<17) { emotion_="playful"; setAction(Action::Walk,3200); }\n    scheduleIdleMoment();\n',
    '    else if(r<17) { emotion_="playful"; setAction(Action::Walk,3200); }\n'
    '    else if(r<20) { emotion_="cozy"; setAction(Action::Stretch,2600); }\n'
    '    else if(r<22) { emotion_="sleepy"; setAction(Action::Yawn,2800); }\n'
    '    scheduleIdleMoment();\n',
)
replace_once(
    cpp,
    '    case Action::AdjustGlasses:return "adjusting glasses"; case Action::RemoveGlasses:return "no-glasses mode"; case Action::Wave:return "waving";\n',
    '    case Action::AdjustGlasses:return "adjusting glasses"; case Action::RemoveGlasses:return "no-glasses mode"; case Action::Wave:return "waving";\n'
    '    case Action::Pat:return "enjoying a head pat"; case Action::Lifted:return "being picked up"; case Action::Landing:return "landing";\n'
    '    case Action::Dizzy:return "dizzy"; case Action::Stretch:return "stretching"; case Action::Yawn:return "yawning"; case Action::CursorWatch:return "watching the cursor";\n',
)

old_mouse = '''void PetWindow::enterEvent(QEnterEvent*){\n    // Hovering should not make Tony constantly wave. Stay calm and let the blink timer work.\n    if(agentState_=="idle" && action_==Action::Idle && !blinkTimer_.isActive()) scheduleBlink();\n}\nvoid PetWindow::leaveEvent(QEvent*){}\n\nvoid PetWindow::mousePressEvent(QMouseEvent *e){\n    if(e->button()==Qt::LeftButton){ dragging_=true; dragOffset_=e->globalPosition().toPoint()-frameGeometry().topLeft(); }\n}\nvoid PetWindow::mouseMoveEvent(QMouseEvent *e){\n    if(dragging_ && (e->buttons()&Qt::LeftButton)){\n        move(e->globalPosition().toPoint()-dragOffset_);\n        action_=Action::Bob; frame_=0;\n        const QPoint anchor=mapToGlobal(QPoint(width()/2,20));\n        bubble_.follow(anchor); composer_.follow(anchor); update();\n    }\n}\nvoid PetWindow::mouseReleaseEvent(QMouseEvent *e){\n    if(e->button()==Qt::LeftButton){ dragging_=false; savePosition(); restoreAgentAction(); }\n}\n'''
new_mouse = '''void PetWindow::enterEvent(QEnterEvent*){\n    // Tony notices the pointer once, then settles again. This is deliberately not a looping hover animation.\n    if(agentState_=="idle" && action_==Action::Idle && !dragging_) {\n        emotion_="curious";\n        setAction(Action::CursorWatch,1300);\n    }\n    if(!blinkTimer_.isActive()) scheduleBlink();\n}\nvoid PetWindow::leaveEvent(QEvent*){}\n\nvoid PetWindow::mousePressEvent(QMouseEvent *e){\n    if(e->button()==Qt::LeftButton){\n        dragging_=true;\n        movedDuringDrag_=false;\n        dragTravel_=0;\n        dragStartGlobal_=e->globalPosition().toPoint();\n        lastDragGlobal_=dragStartGlobal_;\n        dragOffset_=dragStartGlobal_-frameGeometry().topLeft();\n    }\n}\nvoid PetWindow::mouseMoveEvent(QMouseEvent *e){\n    if(dragging_ && (e->buttons()&Qt::LeftButton)){\n        const QPoint current=e->globalPosition().toPoint();\n        dragTravel_ += (current-lastDragGlobal_).manhattanLength();\n        lastDragGlobal_=current;\n        if((current-dragStartGlobal_).manhattanLength()>6) movedDuringDrag_=true;\n        if(movedDuringDrag_) {\n            move(current-dragOffset_);\n            action_=Action::Lifted; frame_=0;\n            const QPoint anchor=mapToGlobal(QPoint(width()/2,20));\n            bubble_.follow(anchor); composer_.follow(anchor); update();\n        }\n    }\n}\nvoid PetWindow::mouseReleaseEvent(QMouseEvent *e){\n    if(e->button()==Qt::LeftButton){\n        dragging_=false;\n        if(movedDuringDrag_) {\n            savePosition();\n            if(dragTravel_>700) {\n                emotion_="dizzy";\n                setAction(Action::Dizzy,2600);\n            } else {\n                emotion_="playful";\n                setAction(Action::Landing,1200);\n            }\n        } else if(agentState_=="idle") {\n            emotion_="happy";\n            setAction(Action::Pat,1700);\n        } else {\n            restoreAgentAction();\n        }\n        movedDuringDrag_=false;\n        dragTravel_=0;\n    }\n}\n'''
replace_once(cpp, old_mouse, new_mouse)

replace_once(
    cpp,
    '    auto sleep=actions->addAction(uiText("Sleep","睡觉"));\n',
    '    auto sleep=actions->addAction(uiText("Sleep","睡觉"));\n'
    '    auto stretch=actions->addAction(uiText("Stretch","伸懒腰"));\n'
    '    auto yawn=actions->addAction(uiText("Yawn","打哈欠"));\n',
)
replace_once(
    cpp,
    '    else if(chosen==sleep) setAction(Action::Sleep);\n',
    '    else if(chosen==sleep) setAction(Action::Sleep);\n'
    '    else if(chosen==stretch) setAction(Action::Stretch,2600);\n'
    '    else if(chosen==yawn) setAction(Action::Yawn,2800);\n',
)

replace_once(cmake, "project(TonyDesktopPet VERSION 0.9.0 LANGUAGES CXX)", "project(TonyDesktopPet VERSION 0.10.0 LANGUAGES CXX)")
replace_once(
    validator,
    '    "remove_glasses", "wave",\n',
    '    "remove_glasses", "wave", "pat", "lifted", "landing", "dizzy",\n'
    '    "stretch", "yawn", "cursor_watch",\n',
)

print("TONY_V010_INTERACTION_PATCH=PASS")
