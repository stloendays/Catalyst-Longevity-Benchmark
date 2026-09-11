#!/usr/bin/env python3
"""Validate optional Tony desktop-pet state PNGs and frame sequences."""

from __future__ import annotations

import hashlib
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE_DIR = ROOT / "assets" / "states"
ANIMATION_DIR = ROOT / "assets" / "animations"
EXPECTED = (
    "idle", "working", "walk", "thinking", "celebrate", "sleep", "shiver",
    "ask_hug", "hug", "blush", "blush_wave", "study", "adjust_glasses",
    "remove_glasses", "wave",
)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def verify_png_chunks(path: Path) -> None:
    data = path.read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError("not a PNG")
    import zlib
    pos = 8
    saw_idat = False
    saw_iend = False
    while pos + 12 <= len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        chunk_type = data[pos + 4:pos + 8]
        end = pos + 12 + length
        if end > len(data):
            raise ValueError(f"truncated {chunk_type!r} chunk")
        payload = data[pos + 8:pos + 8 + length]
        expected_crc = struct.unpack(">I", data[pos + 8 + length:end])[0]
        actual_crc = zlib.crc32(chunk_type)
        actual_crc = zlib.crc32(payload, actual_crc) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise ValueError(f"CRC mismatch in {chunk_type.decode('latin1')} chunk")
        if chunk_type == b"IDAT":
            saw_idat = True
        if chunk_type == b"IEND":
            saw_iend = True
            if end != len(data):
                raise ValueError("unexpected bytes after IEND")
            break
        pos = end
    if not saw_idat or not saw_iend:
        raise ValueError("missing IDAT or IEND")


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


def validate_png(path: Path, failures: list[str], *, min_size: int = 180) -> tuple[int, int] | None:
    try:
        verify_png_chunks(path)
        w, h, color_type, digest = inspect_png(path)
    except Exception as exc:
        failures.append(f"{path}: {exc}")
        return None
    if w != h:
        failures.append(f"{path}: canvas must be square, got {w}x{h}")
    if w < min_size:
        failures.append(f"{path}: runtime artwork is too small ({w}px)")
    if color_type not in {3, 4, 6}:
        failures.append(f"{path}: expected transparency-capable PNG color type, got {color_type}")
    print(f"OK       {path.relative_to(ROOT)!s:48s} {w}x{h} type={color_type} sha256={digest}")
    return w, h


def main() -> int:
    failures: list[str] = []
    present = 0
    for key in EXPECTED:
        path = STATE_DIR / f"{key}.png"
        if not path.exists():
            print(f"MISSING  states/{key}.png  (legal: runtime falls back)")
            continue
        present += 1
        validate_png(path, failures)

    animated_present = 0
    for key in EXPECTED:
        folder = ANIMATION_DIR / key
        if not folder.exists():
            print(f"MISSING  animations/{key}/  (legal: static state image is used)")
            continue
        frames = sorted(folder.glob("frame_*.png"))
        if not frames:
            failures.append(f"{folder}: animation directory exists but contains no frames")
            continue
        expected_names = [f"frame_{i:02d}.png" for i in range(1, len(frames) + 1)]
        actual_names = [p.name for p in frames]
        if actual_names != expected_names:
            failures.append(f"{folder}: frames must be contiguous: {expected_names}, got {actual_names}")
        if not 2 <= len(frames) <= 12:
            failures.append(f"{folder}: expected 2-12 frames, got {len(frames)}")

        geometry = None
        for frame in frames:
            current = validate_png(frame, failures)
            if current is None:
                continue
            if geometry is None:
                geometry = current
            elif current != geometry:
                failures.append(f"{folder}: frame geometry must stay constant; expected {geometry}, got {current} in {frame.name}")
        animated_present += 1

    unknown = []
    if ANIMATION_DIR.exists():
        for child in ANIMATION_DIR.iterdir():
            if child.is_dir() and child.name not in EXPECTED:
                unknown.append(child.name)
    if unknown:
        failures.append(f"unknown animation state directories: {', '.join(sorted(unknown))}")

    print(f"TONY_STATE_ASSETS_PRESENT={present}/{len(EXPECTED)}")
    print(f"TONY_ANIMATIONS_PRESENT={animated_present}/{len(EXPECTED)}")
    if failures:
        for item in failures:
            print("ERROR   ", item)
        return 1
    print("TONY_STATE_ASSET_VALIDATION=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
