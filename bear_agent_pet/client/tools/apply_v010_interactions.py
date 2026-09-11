#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "PetWindow.h"
CPP = ROOT / "src" / "PetWindowV7.cpp"
CMAKE = ROOT / "CMakeLists.txt"
VALIDATOR = ROOT / "tools" / "validate_state_assets.py"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"Expected patch anchor not found for {label}: {old[:140]!r}")
    return text.replace(old, new, 1)


def patch_header() -> None:
    text = HEADER.read_text(encoding="utf-8")
    if "Pat, Lifted, Landing" not in text:
        text = replace_once(
            text,
            "        AdjustGlasses, RemoveGlasses, Wave\n",
            "        AdjustGlasses, RemoveGlasses, Wave,\n"
            "        Pat, Lifted, Landing, Dizzy, Stretch, Yawn, CursorWatch\n",
            "PetWindow Action enum",
        )
    if "movedDuringDrag_" not in text:
        text = replace_once(
            text,
            "    bool dragging_{false};\n    bool idleBlinking_{false};\n",
            "    bool dragging_{false};\n"
            "    bool movedDuringDrag_{false};\n"
            "    QPoint dragStartGlobal_;\n"
            "    QPoint lastDragGlobal_;\n"
            "    int dragTravel_{0};\n"
            "    bool idleBlinking_{false};\n",
            "PetWindow drag fields",
        )
    HEADER.write_text(text, encoding="utf-8")


def patch_cpp() -> None:
    text = CPP.read_text(encoding="utf-8")

    if '"pat","lifted","landing"' not in text:
        text = replace_once(
            text,
            '        "ask_hug","hug","blush","blush_wave","study","adjust_glasses","remove_glasses","wave"\n',
            '        "ask_hug","hug","blush","blush_wave","study","adjust_glasses","remove_glasses","wave",\n'
            '        "pat","lifted","landing","dizzy","stretch","yawn","cursor_watch"\n',
            "asset key list",
        )

    if "case Action::Pat:return \"pat\";" not in text:
        text = replace_once(
            text,
            '    case Action::Wave:return "wave";\n',
            '    case Action::Wave:return "wave";\n'
            '    case Action::Pat:return "pat";\n'
            '    case Action::Lifted:return "lifted";\n'
            '    case Action::Landing:return "landing";\n'
            '    case Action::Dizzy:return "dizzy";\n'
            '    case Action::Stretch:return "stretch";\n'
            '    case Action::Yawn:return "yawn";\n'
            '    case Action::CursorWatch:return "cursor_watch";\n',
            "assetKeyForAction",
        )

    if "case Action::Pat:return 6;" not in text:
        text = replace_once(
            text,
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
            "frameStrideForAction",
        )

    old_pixmap = '''const QPixmap *PetWindow::pixmapForAction(Action action) const {\n    const QString key=assetKeyForAction(action);\n\n    // Visual-quality rollback: the old multi-frame PNG sequences were authored\n    // with inconsistent crops and character proportions. They made Tony appear\n    // to lose feet/ears/arms between frames. Until a single approved master\n    // character sheet is available, use the higher-quality complete state art\n    // and let paintEvent provide the motion. This keeps Tony visually stable.\n    auto it=stateAssets_.constFind(key);\n    if(it!=stateAssets_.constEnd() && !it.value().isNull()) return &it.value();\n\n    it=stateAssets_.constFind("idle");\n    if(it!=stateAssets_.constEnd() && !it.value().isNull()) return &it.value();\n\n    return pet_.isNull() ? nullptr : &pet_;\n}\n'''
    new_pixmap = '''const QPixmap *PetWindow::pixmapForAction(Action action) const {\n    const QString key=assetKeyForAction(action);\n\n    // Preserve the stable full-character state art for the original actions.\n    // Only the seven v0.10 interaction sequences were authored as approved,\n    // full-character animation frames, so only those actions consume frames.\n    const bool useAuthoredFrames =\n        action==Action::Pat || action==Action::Lifted || action==Action::Landing ||\n        action==Action::Dizzy || action==Action::Stretch || action==Action::Yawn ||\n        action==Action::CursorWatch;\n    if(useAuthoredFrames) {\n        const auto framesIt=animationAssets_.constFind(key);\n        if(framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty()) {\n            const int stride=qMax(1,frameStrideForAction(action));\n            const int index=(frame_/stride)%framesIt.value().size();\n            return &framesIt.value().at(index);\n        }\n    }\n\n    auto it=stateAssets_.constFind(key);\n    if(it!=stateAssets_.constEnd() && !it.value().isNull()) return &it.value();\n\n    it=stateAssets_.constFind("idle");\n    if(it!=stateAssets_.constEnd() && !it.value().isNull()) return &it.value();\n\n    return pet_.isNull() ? nullptr : &pet_;\n}\n'''
    if "const bool useAuthoredFrames" not in text:
        text = replace_once(text, old_pixmap, new_pixmap, "pixmapForAction")

    if "case Action::Pat: break;" not in text:
        text = replace_once(
            text,
            '    case Action::Wave: dy=-qAbs(int(3*qSin(t*1.6))); rotation=3.0*qSin(t*1.8); break;\n',
            '    case Action::Wave: dy=-qAbs(int(3*qSin(t*1.6))); rotation=3.0*qSin(t*1.8); break;\n'
            '    case Action::Pat: break;\n'
            '    case Action::Lifted: dy=-4; break;\n'
            '    case Action::Landing: break;\n'
            '    case Action::Dizzy: rotation=1.2*qSin(t*1.4); break;\n'
            '    case Action::Stretch: break;\n'
            '    case Action::Yawn: break;\n'
            '    case Action::CursorWatch: break;\n',
            "paintEvent new actions",
        )

    if 'n=="pat" || n=="head_pat"' not in text:
        text = replace_once(
            text,
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
            "actionFromWire new aliases",
        )

    if 'setAction(Action::Stretch,2600)' not in text:
        text = replace_once(
            text,
            '    else if(r<17) { emotion_="playful"; setAction(Action::Walk,4800); }\n    scheduleIdleMoment();\n',
            '    else if(r<17) { emotion_="playful"; setAction(Action::Walk,4800); }\n'
            '    else if(r<19) { emotion_="cozy"; setAction(Action::Stretch,2600); }\n'
            '    else if(r<20) { emotion_="sleepy"; setAction(Action::Yawn,2800); }\n'
            '    scheduleIdleMoment();\n',
            "rare idle stretch/yawn",
        )

    if 'case Action::Pat:return "enjoying a head pat";' not in text:
        text = replace_once(
            text,
            '    case Action::AdjustGlasses:return "adjusting glasses"; case Action::RemoveGlasses:return "no-glasses mode"; case Action::Wave:return "waving";\n',
            '    case Action::AdjustGlasses:return "adjusting glasses"; case Action::RemoveGlasses:return "no-glasses mode"; case Action::Wave:return "waving";\n'
            '    case Action::Pat:return "enjoying a head pat"; case Action::Lifted:return "being picked up"; case Action::Landing:return "landing";\n'
            '    case Action::Dizzy:return "dizzy"; case Action::Stretch:return "stretching"; case Action::Yawn:return "yawning"; case Action::CursorWatch:return "watching the cursor";\n',
            "actionName new actions",
        )

    old_mouse = '''void PetWindow::enterEvent(QEnterEvent*){\n    // Hovering should not make Tony constantly wave. Stay calm and let the blink timer work.\n    if(agentState_=="idle" && action_==Action::Idle && !blinkTimer_.isActive()) scheduleBlink();\n}\nvoid PetWindow::leaveEvent(QEvent*){}\n\nvoid PetWindow::mousePressEvent(QMouseEvent *e){\n    if(e->button()==Qt::LeftButton){ dragging_=true; dragOffset_=e->globalPosition().toPoint()-frameGeometry().topLeft(); }\n}\nvoid PetWindow::mouseMoveEvent(QMouseEvent *e){\n    if(dragging_ && (e->buttons()&Qt::LeftButton)){\n        move(e->globalPosition().toPoint()-dragOffset_);\n        action_=Action::Bob; frame_=0;\n        const QPoint anchor=mapToGlobal(QPoint(width()/2,20));\n        bubble_.follow(anchor); composer_.follow(anchor); update();\n    }\n}\nvoid PetWindow::mouseReleaseEvent(QMouseEvent *e){\n    if(e->button()==Qt::LeftButton){ dragging_=false; savePosition(); restoreAgentAction(); }\n}\n'''
    new_mouse = '''void PetWindow::enterEvent(QEnterEvent*){\n    // Tony notices the pointer once and then settles again; this is not a loop.\n    if(agentState_=="idle" && action_==Action::Idle && !dragging_) {\n        emotion_="curious";\n        setAction(Action::CursorWatch,1300);\n    }\n    if(!blinkTimer_.isActive()) scheduleBlink();\n}\nvoid PetWindow::leaveEvent(QEvent*){}\n\nvoid PetWindow::mousePressEvent(QMouseEvent *e){\n    if(e->button()==Qt::LeftButton){\n        dragging_=true;\n        movedDuringDrag_=false;\n        dragTravel_=0;\n        dragStartGlobal_=e->globalPosition().toPoint();\n        lastDragGlobal_=dragStartGlobal_;\n        dragOffset_=dragStartGlobal_-frameGeometry().topLeft();\n    }\n}\nvoid PetWindow::mouseMoveEvent(QMouseEvent *e){\n    if(dragging_ && (e->buttons()&Qt::LeftButton)){\n        const QPoint current=e->globalPosition().toPoint();\n        dragTravel_ += (current-lastDragGlobal_).manhattanLength();\n        lastDragGlobal_=current;\n        if((current-dragStartGlobal_).manhattanLength()>6) movedDuringDrag_=true;\n        if(movedDuringDrag_) {\n            move(current-dragOffset_);\n            action_=Action::Lifted; frame_=0;\n            const QPoint anchor=mapToGlobal(QPoint(width()/2,20));\n            bubble_.follow(anchor); composer_.follow(anchor); update();\n        }\n    }\n}\nvoid PetWindow::mouseReleaseEvent(QMouseEvent *e){\n    if(e->button()==Qt::LeftButton){\n        dragging_=false;\n        if(movedDuringDrag_) {\n            savePosition();\n            if(dragTravel_>700) {\n                emotion_="dizzy";\n                setAction(Action::Dizzy,2600);\n            } else {\n                emotion_="playful";\n                setAction(Action::Landing,1200);\n            }\n        } else if(agentState_=="idle") {\n            emotion_="happy";\n            setAction(Action::Pat,1700);\n        } else {\n            restoreAgentAction();\n        }\n        movedDuringDrag_=false;\n        dragTravel_=0;\n    }\n}\n'''
    if "movedDuringDrag_=false;" not in text:
        text = replace_once(text, old_mouse, new_mouse, "mouse interactions")

    if 'auto stretch=actions->addAction' not in text:
        text = replace_once(
            text,
            '    auto sleep=actions->addAction(uiText("Sleep","睡觉"));\n',
            '    auto sleep=actions->addAction(uiText("Sleep","睡觉"));\n'
            '    auto stretch=actions->addAction(uiText("Stretch","伸懒腰"));\n'
            '    auto yawn=actions->addAction(uiText("Yawn","打哈欠"));\n',
            "context menu actions",
        )
        text = replace_once(
            text,
            '    else if(chosen==sleep) setAction(Action::Sleep);\n',
            '    else if(chosen==sleep) setAction(Action::Sleep);\n'
            '    else if(chosen==stretch) setAction(Action::Stretch,2600);\n'
            '    else if(chosen==yawn) setAction(Action::Yawn,2800);\n',
            "context menu dispatch",
        )

    CPP.write_text(text, encoding="utf-8")


def patch_validator() -> None:
    text = VALIDATOR.read_text(encoding="utf-8")
    if "NEW_INTERACTIONS" not in text:
        text = replace_once(
            text,
            '    "remove_glasses", "wave",\n)\n',
            '    "remove_glasses", "wave", "pat", "lifted", "landing", "dizzy",\n'
            '    "stretch", "yawn", "cursor_watch",\n)\n'
            'NEW_INTERACTIONS = {"pat", "lifted", "landing", "dizzy", "stretch", "yawn", "cursor_watch"}\n',
            "validator expected actions",
        )
        text = replace_once(
            text,
            '            current = validate_png(frame, failures)\n',
            '            current = validate_png(frame, failures, min_size=96 if key in NEW_INTERACTIONS else 180)\n',
            "validator animation size policy",
        )
    VALIDATOR.write_text(text, encoding="utf-8")


def patch_cmake() -> None:
    text = CMAKE.read_text(encoding="utf-8")
    text, count = re.subn(
        r"project\(TonyDesktopPet VERSION [0-9]+\.[0-9]+\.[0-9]+ LANGUAGES CXX\)",
        "project(TonyDesktopPet VERSION 0.10.0 LANGUAGES CXX)",
        text,
        count=1,
    )
    if count != 1:
        raise SystemExit("Could not update TonyDesktopPet version")
    CMAKE.write_text(text, encoding="utf-8")


def main() -> int:
    patch_header()
    patch_cpp()
    patch_validator()
    patch_cmake()
    print("TONY_V010_INTERACTION_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
