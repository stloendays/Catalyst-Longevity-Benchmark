#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CPP = ROOT / "bear_agent_pet/client/src/PetWindowV7.cpp"
CMAKE = ROOT / "bear_agent_pet/client/CMakeLists.txt"
VALIDATOR = ROOT / "bear_agent_pet/client/tools/validate_state_assets.py"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"Missing patch anchor: {label}")
    return text.replace(old, new, 1)

cpp = CPP.read_text(encoding="utf-8")
cpp = replace_once(
    cpp,
    "animTimer_.setInterval(70);",
    "animTimer_.setInterval(110);",
    "animation cadence",
)
cpp = replace_once(
    cpp,
    "// Natural but quiet: roughly one blink every 5-11 seconds.\n    blinkTimer_.start(QRandomGenerator::global()->bounded(5000,11001));",
    "// Natural but quiet: keep long pauses so Tony does not look restless.\n    blinkTimer_.start(QRandomGenerator::global()->bounded(7500,15001));",
    "blink interval",
)
cpp = replace_once(
    cpp,
    "// Autonomous gestures are now rare; the usual state is simply sitting and blinking.\n    idleTimer_.start(QRandomGenerator::global()->bounded(90000,210001));",
    "// Autonomous gestures stay rare. Tony should spend most of his time simply sitting.\n    idleTimer_.start(QRandomGenerator::global()->bounded(140000,320001));",
    "idle gesture interval",
)
cpp = replace_once(
    cpp,
    "// Slow desktop walk: one pixel per animation tick rather than gliding constantly.\n        QPoint n=pos()+QPoint(1*walkDirection_,0);",
    "// Slow desktop walk: advance only on every second animation tick.\n        // This preserves the original native-motion feel without making Tony pace.\n        QPoint n=pos()+QPoint((frame_%2==0 ? 1 : 0)*walkDirection_,0);",
    "walk pacing",
)
cpp = replace_once(cpp, "setAction(Action::Wave,900);", "setAction(Action::Wave,1500);", "connect wave")
cpp = replace_once(cpp, "setAction(Action::Celebrate,1400);", "setAction(Action::Celebrate,2100);", "pair celebrate")
cpp = replace_once(cpp, "setAction(Action::Walk,3000);", "setAction(Action::Walk,4800);", "manual walk")
cpp = replace_once(cpp, "setAction(Action::AdjustGlasses,1600);", "setAction(Action::AdjustGlasses,2300);", "manual glasses")
cpp = replace_once(cpp, "setAction(Action::RemoveGlasses,2600);", "setAction(Action::RemoveGlasses,3400);", "manual remove glasses")
cpp = replace_once(cpp, "setAction(Action::Shiver,2200); showBubble", "setAction(Action::Shiver,3000); showBubble", "manual shiver")
cpp = replace_once(cpp, "void PetWindow::hugTony(){ emotion_=\"happy\"; setAction(Action::Hug,2600);", "void PetWindow::hugTony(){ emotion_=\"happy\"; setAction(Action::Hug,3400);", "hug duration")
cpp = replace_once(cpp, "setAction(Action::AskHug,2600); showBubble", "setAction(Action::AskHug,3400); showBubble", "idle ask hug")
cpp = replace_once(cpp, "setAction(Action::Wave,1400);", "setAction(Action::Wave,1900);", "idle wave")
cpp = replace_once(cpp, "setAction(Action::AdjustGlasses,1600);", "setAction(Action::AdjustGlasses,2300);", "idle glasses")
cpp = replace_once(cpp, "setAction(Action::Walk,3200);", "setAction(Action::Walk,4800);", "idle walk")
CPP.write_text(cpp, encoding="utf-8")

cmake = CMAKE.read_text(encoding="utf-8")
if "project(TonyDesktopPet VERSION 0.9.1 LANGUAGES CXX)" in cmake:
    cmake = cmake.replace(
        "project(TonyDesktopPet VERSION 0.9.1 LANGUAGES CXX)",
        "project(TonyDesktopPet VERSION 0.9.2 LANGUAGES CXX)",
        1,
    )
elif "project(TonyDesktopPet VERSION 0.9.2 LANGUAGES CXX)" not in cmake:
    raise SystemExit("Unexpected Tony version; refusing unsafe rewrite")
CMAKE.write_text(cmake, encoding="utf-8")

validator = VALIDATOR.read_text(encoding="utf-8")
if "def verify_png_chunks" not in validator:
    marker = "PNG_SIGNATURE = b\"\\x89PNG\\r\\n\\x1a\\n\"\n\n\n"
    addition = '''PNG_SIGNATURE = b"\\x89PNG\\r\\n\\x1a\\n"\n\n\ndef verify_png_chunks(path: Path) -> None:\n    data = path.read_bytes()\n    if not data.startswith(PNG_SIGNATURE):\n        raise ValueError("not a PNG")\n    import zlib\n    pos = 8\n    saw_idat = False\n    saw_iend = False\n    while pos + 12 <= len(data):\n        length = struct.unpack(">I", data[pos:pos + 4])[0]\n        chunk_type = data[pos + 4:pos + 8]\n        end = pos + 12 + length\n        if end > len(data):\n            raise ValueError(f"truncated {chunk_type!r} chunk")\n        payload = data[pos + 8:pos + 8 + length]\n        expected_crc = struct.unpack(">I", data[pos + 8 + length:end])[0]\n        actual_crc = zlib.crc32(chunk_type)\n        actual_crc = zlib.crc32(payload, actual_crc) & 0xFFFFFFFF\n        if actual_crc != expected_crc:\n            raise ValueError(f"CRC mismatch in {chunk_type.decode('latin1')} chunk")\n        if chunk_type == b"IDAT":\n            saw_idat = True\n        if chunk_type == b"IEND":\n            saw_iend = True\n            if end != len(data):\n                raise ValueError("unexpected bytes after IEND")\n            break\n        pos = end\n    if not saw_idat or not saw_iend:\n        raise ValueError("missing IDAT or IEND")\n\n\n'''
    if marker not in validator:
        raise SystemExit("PNG validator marker missing")
    validator = validator.replace(marker, addition, 1)
    validator = validator.replace(
        "        w, h, color_type, digest = inspect_png(path)\n",
        "        verify_png_chunks(path)\n        w, h, color_type, digest = inspect_png(path)\n",
        1,
    )
VALIDATOR.write_text(validator, encoding="utf-8")

print("TONY_NATIVE_MOTION_TUNING=PASS")
print("- 110 ms animation cadence")
print("- 7.5-15 s micro-expression spacing")
print("- 140-320 s autonomous-action spacing")
print("- walk advances every second tick")
print("- key native actions last longer and read more clearly")
print("- PNG validation now checks chunk CRC/integrity")
