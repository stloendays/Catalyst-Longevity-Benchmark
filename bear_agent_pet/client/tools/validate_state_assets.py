#!/usr/bin/env python3
"""Validate optional Tony desktop-pet state PNGs without external packages."""

from __future__ import annotations

import hashlib
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE_DIR = ROOT / "assets" / "states"
EXPECTED = (
    "idle", "working", "walk", "thinking", "celebrate", "sleep", "shiver",
    "ask_hug", "hug", "blush", "study", "adjust_glasses", "remove_glasses", "wave",
)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def inspect_png(path: Path) -> tuple[int, int, int, str]:
    data = path.read_bytes()
    if len(data) < 33 or data[:8] != PNG_SIGNATURE:
        raise ValueError("not a PNG")
    length = struct.unpack(">I", data[8:12])[0]
    if data[12:16] != b"IHDR" or length != 13:
        raise ValueError("invalid IHDR")
    width, height, bit_depth, color_type = struct.unpack(">IIBB", data[16:26])
    digest = hashlib.sha256(data).hexdigest()[:16]
    return width, height, color_type, digest


def main() -> int:
    failures: list[str] = []
    present = 0
    for key in EXPECTED:
        path = STATE_DIR / f"{key}.png"
        if not path.exists():
            print(f"MISSING  {key}.png  (legal: runtime falls back)")
            continue
        present += 1
        try:
            w, h, color_type, digest = inspect_png(path)
        except Exception as exc:
            failures.append(f"{path.name}: {exc}")
            continue
        if w != h:
            failures.append(f"{path.name}: canvas must be square, got {w}x{h}")
        if w < 180:
            failures.append(f"{path.name}: runtime artwork is too small ({w}px)")
        # PNG color types 4 and 6 have an alpha channel. Type 3 may use tRNS,
        # which is also acceptable for optimized palette sprites.
        if color_type not in {3, 4, 6}:
            failures.append(f"{path.name}: expected transparency-capable PNG color type, got {color_type}")
        print(f"OK       {path.name:22s} {w}x{h} type={color_type} sha256={digest}")

    print(f"TONY_STATE_ASSETS_PRESENT={present}/{len(EXPECTED)}")
    if failures:
        for item in failures:
            print("ERROR   ", item)
        return 1
    print("TONY_STATE_ASSET_VALIDATION=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
