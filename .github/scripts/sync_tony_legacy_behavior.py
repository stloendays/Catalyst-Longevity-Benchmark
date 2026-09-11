#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CLIENT = ROOT / "bear_agent_pet" / "client"
HEADER = CLIENT / "src" / "PetWindow.h"
CPP = CLIENT / "src" / "PetWindowV7.cpp"
CMAKE = CLIENT / "CMakeLists.txt"
VERSION = CLIENT / "VERSION"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"Missing migration anchor for {label}: {old[:120]!r}")
    return text.replace(old, new, 1)


def replace_function(text: str, signature: str, replacement: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise SystemExit(f"Could not find function {signature}")
    brace = text.find("{", start)
    if brace < 0:
        raise SystemExit(f"Could not find opening brace for {signature}")
    depth = 0
    end = None
    for i in range(brace, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    if end is None:
        raise SystemExit(f"Could not find end of {signature}")
    return text[:start] + replacement.rstrip() + "\n" + text[end:]


def patch_header() -> None:
    text = HEADER.read_text(encoding="utf-8")
    if "TonyBehaviorEngine.h" not in text or "tickPhysics" not in text:
        raise SystemExit("Expected V0.8.3 behavior header was not staged")

    # configureConnection used to be private; the V0.9 settings surface calls it.
    text = text.replace("    void configureConnection();\n", "")
    public_anchor = "public:\n    explicit PetWindow(QWidget *parent=nullptr);\n    ~PetWindow() override;\n"
    public_block = public_anchor + "\npublic slots:\n    void configureConnection();\n    void applyUiLanguage(const QString &language);\n    void syncOperatorLogsForUpdate(const QString &targetVersion);\n"
    text = replace_once(text, public_anchor, public_block, "public settings slots")
    HEADER.write_text(text, encoding="utf-8")


def patch_cpp() -> None:
    text = CPP.read_text(encoding="utf-8")
    if "void PetWindow::tickPhysics()" not in text or "void PetWindow::walkAlongForegroundWindow()" not in text:
        raise SystemExit("Expected V0.8.3 desktop physics source was not staged")

    # Keep the quieter V0.9 render cadence while restoring the richer behavior state machine.
    text = text.replace("animTimer_.setInterval(70);", "animTimer_.setInterval(110);", 1)

    # Load both the V0.8 state names and the approved V0.10 interaction aliases.
    old_keys = '''        "idle","curious","pet","carried","land","dizzy","stretch","yawn",\n        "working","walk","thinking","celebrate","sleep","shiver",\n        "ask_hug","hug","blush","blush_wave","study","adjust_glasses","remove_glasses","wave"\n'''
    new_keys = '''        "idle","curious","pet","carried","land","dizzy","stretch","yawn",\n        "working","walk","thinking","celebrate","sleep","shiver",\n        "ask_hug","hug","blush","blush_wave","study","adjust_glasses","remove_glasses","wave",\n        "pat","lifted","landing","cursor_watch"\n'''
    if '"pat","lifted","landing","cursor_watch"' not in text:
        text = replace_once(text, old_keys, new_keys, "visual alias keys")

    pixmap = r'''const QPixmap *PetWindow::pixmapForAction(Action action) const {
    const QString key=assetKeyForAction(action);

    // Idle may use only the short face-blink sequence. Do not loop the legacy
    // full-body animation sets: their crops/proportions vary and caused Tony to
    // appear to lose ears, feet or arms between frames.
    if(action==Action::Idle) {
        auto framesIt=animationAssets_.constFind("idle");
        if(idleBlinking_ && framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty()) {
            static constexpr int blinkSequence[] = {0, 1, 2, 2, 1, 0, 0};
            const int sequenceSize=static_cast<int>(sizeof(blinkSequence)/sizeof(blinkSequence[0]));
            const int step=qBound(0,idleBlinkTick_,sequenceSize-1);
            const int index=blinkSequence[step] % framesIt.value().size();
            return &framesIt.value().at(index);
        }
        auto idleIt=stateAssets_.constFind("idle");
        if(idleIt!=stateAssets_.constEnd() && !idleIt.value().isNull()) return &idleIt.value();
    }

    // Prefer the complete V0.9 state image. V0.8-only behaviors can consume the
    // newer approved interaction sequences when those assets are present.
    auto stateIt=stateAssets_.constFind(key);
    if(stateIt!=stateAssets_.constEnd() && !stateIt.value().isNull()) return &stateIt.value();

    QString authoredAlias;
    switch(action) {
    case Action::Curious: authoredAlias="cursor_watch"; break;
    case Action::Pet: authoredAlias="pat"; break;
    case Action::Carried: authoredAlias="lifted"; break;
    case Action::Land: authoredAlias="landing"; break;
    case Action::Dizzy: authoredAlias="dizzy"; break;
    case Action::Stretch: authoredAlias="stretch"; break;
    case Action::Yawn: authoredAlias="yawn"; break;
    default: break;
    }
    if(!authoredAlias.isEmpty()) {
        auto framesIt=animationAssets_.constFind(authoredAlias);
        if(framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty()) {
            const int stride=qMax(1,frameStrideForAction(action));
            const int index=(frame_/stride)%framesIt.value().size();
            return &framesIt.value().at(index);
        }
        auto aliasState=stateAssets_.constFind(authoredAlias);
        if(aliasState!=stateAssets_.constEnd() && !aliasState.value().isNull()) return &aliasState.value();
    }

    // Semantic fallbacks keep the same approved Tony design even before a
    // dedicated asset exists for every restored behavior.
    QString fallback="idle";
    if(action==Action::Pet) fallback="blush";
    else if(action==Action::Dizzy) fallback="thinking";
    else if(action==Action::Yawn) fallback="sleep";
    auto fallbackIt=stateAssets_.constFind(fallback);
    if(fallbackIt!=stateAssets_.constEnd() && !fallbackIt.value().isNull()) return &fallbackIt.value();

    auto idleIt=stateAssets_.constFind("idle");
    if(idleIt!=stateAssets_.constEnd() && !idleIt.value().isNull()) return &idleIt.value();
    return pet_.isNull() ? nullptr : &pet_;
}'''
    text = replace_function(text, "const QPixmap *PetWindow::pixmapForAction(Action action) const", pixmap)

    # Route the context-menu language change through the V0.9 logging-aware slot.
    old_language = '''    else if(chosen==english || chosen==chinese) {\n        const QString lang=(chosen==chinese) ? "zh" : "en";\n        QSettings().setValue("ui/language",lang);\n        agent_.setLanguage(lang);\n        composer_.setLanguage(lang);\n        showBubble(lang=="zh" ? "语言已切换为简体中文。" : "Language changed to English.",3200);\n    }\n'''
    new_language = '''    else if(chosen==english || chosen==chinese) {\n        const QString lang=(chosen==chinese) ? "zh" : "en";\n        applyUiLanguage(lang);\n    }\n'''
    if old_language in text:
        text = text.replace(old_language, new_language, 1)

    CPP.write_text(text, encoding="utf-8")


def patch_cmake() -> None:
    text = CMAKE.read_text(encoding="utf-8")
    if "src/TonyBehaviorEngine.cpp" not in text:
        anchor = "    src/PetWindow.h\n"
        addition = "    src/PetWindow.h\n    src/TonyBehaviorEngine.cpp\n    src/TonyBehaviorEngine.h\n"
        text = replace_once(text, anchor, addition, "TonyBehaviorEngine CMake sources")
    CMAKE.write_text(text, encoding="utf-8")


def main() -> int:
    patch_header()
    patch_cpp()
    patch_cmake()
    VERSION.write_text("0.9.4\n", encoding="utf-8")

    header = HEADER.read_text(encoding="utf-8")
    cpp = CPP.read_text(encoding="utf-8")
    cmake = CMAKE.read_text(encoding="utf-8")
    required = {
        "life engine": "TonyBehaviorEngine behavior_" in header,
        "physics": "void tickPhysics();" in header and "void PetWindow::tickPhysics()" in cpp,
        "window walking": "walkAlongForegroundWindow" in cpp,
        "collision": "wouldHitForegroundWindow" in cpp,
        "fast cursor chase": "fastCursorChase_" in header and "cursorTravel>=150" in cpp,
        "auto rest": "moveToRestCorner" in cpp,
        "edge peek": "Action::Peek" in cpp,
        "multi-screen": "moveToNextScreen" in cpp,
        "fullscreen": "borderless && coversMonitor" in cpp,
        "settings slots": "syncOperatorLogsForUpdate" in header,
        "canonical build": "src/TonyBehaviorEngine.cpp" in cmake,
    }
    missing = [name for name, ok in required.items() if not ok]
    if missing:
        raise SystemExit("Legacy sync incomplete: " + ", ".join(missing))
    print("TONY_LEGACY_BEHAVIOR_SYNC=PASS version=0.9.4")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
