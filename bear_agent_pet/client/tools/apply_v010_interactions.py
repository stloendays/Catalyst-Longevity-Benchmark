#!/usr/bin/env python3
"""Finalize optional Tony interaction artwork against the current behavior model.

The filename is historical: the interaction-art bootstrap was first prepared as
"v0.10" work. Product versioning is now controlled exclusively by ../VERSION,
so this script must never rewrite CMake or invent a second application version.

V0.9.4 already contains the richer V0.8.3 behavior semantics (Pet, Carried,
Land, Dizzy, Stretch, Yawn, Curious/Peek). Approved artwork folders use the
aliases pat/lifted/landing/dizzy/stretch/yawn/cursor_watch; PetWindow maps those
assets onto the existing behavior states when they are present.
"""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "PetWindow.h"
CPP = ROOT / "src" / "PetWindowV7.cpp"
VERSION = ROOT / "VERSION"
VALIDATOR = ROOT / "tools" / "validate_state_assets.py"

INTERACTION_ALIASES = (
    "pat",
    "lifted",
    "landing",
    "dizzy",
    "stretch",
    "yawn",
    "cursor_watch",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def main() -> int:
    header = HEADER.read_text(encoding="utf-8")
    cpp = CPP.read_text(encoding="utf-8")
    validator = VALIDATOR.read_text(encoding="utf-8")
    version = VERSION.read_text(encoding="utf-8").strip()

    # Behavior semantics come from the restored desktop-life state machine.
    # Do not add duplicate Pat/Lifted/Landing enum values on top of Pet/Carried/Land.
    for token in (
        "Curious", "Peek", "Pet", "Carried", "Land", "Dizzy",
        "Stretch", "Yawn",
    ):
        require(token in header, f"Tony behavior state missing before artwork integration: {token}")

    # PetWindow must know the optional approved asset aliases before bootstrap files
    # are removed. Missing folders are legal; the complete V0.9 state art remains
    # the fallback so Tony never loses body parts because of incomplete sequences.
    for alias in INTERACTION_ALIASES:
        require(alias in cpp, f"Tony runtime does not recognize interaction alias: {alias}")
        require(alias in validator, f"Tony asset validator does not recognize interaction alias: {alias}")

    require("TonyBehaviorEngine behavior_" in header, "TonyBehaviorEngine was not preserved")
    require("void PetWindow::tickPhysics()" in cpp, "Tony desktop physics was not preserved")
    require("void PetWindow::walkAlongForegroundWindow()" in cpp, "Tony window walking was not preserved")
    require("borderless && coversMonitor" in cpp, "Tony hardened fullscreen detection was not preserved")
    require(version, "Canonical Tony VERSION is empty")

    print(f"TONY_INTERACTION_INTEGRATION=PASS canonical_version={version}")
    print("TONY_INTERACTION_VERSION_SOURCE=VERSION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
