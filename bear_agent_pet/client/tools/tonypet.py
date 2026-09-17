#!/usr/bin/env python3
"""Validate, pack, and safely unpack Tony Pet packages.

A .tonypet file is a ZIP archive with pet.json at its root.  The package is
content-only: credentials, logs, memories, machine settings, and executable
code are deliberately rejected.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import zipfile
from pathlib import Path, PurePosixPath

SCHEMA_VERSION = 1
MAX_FILE_BYTES = 32 * 1024 * 1024
MAX_PACKAGE_BYTES = 256 * 1024 * 1024
ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,63}$")
SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$")
IMAGE_EXTENSIONS = {".png", ".gif", ".webp", ".ppm", ".pgm"}
AUDIO_EXTENSIONS = {".wav", ".mp3", ".ogg"}
FORBIDDEN_SUFFIXES = {
    ".exe", ".dll", ".bat", ".cmd", ".ps1", ".sh", ".com", ".msi",
    ".pfx", ".p12", ".pem", ".key", ".crt", ".cer",
}
FORBIDDEN_NAMES = {
    ".env", "settings.ini", "credentials.json", "secrets.json", "token.json",
    "operator.log", "operator_logs.jsonl", "memory.json", "memories.json",
}
FORBIDDEN_PARTS = {
    "operator_logs", "logs", "credentials", "secrets", "tokens", "memory",
    "memories", ".git", ".github",
}


class PetPackageError(ValueError):
    pass


def fail(message: str) -> None:
    raise PetPackageError(message)


def load_manifest(root: Path) -> dict:
    manifest_path = root / "pet.json"
    if not manifest_path.is_file():
        fail("pet.json is missing from the package root")
    try:
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"could not read pet.json: {exc}")
    if not isinstance(data, dict):
        fail("pet.json must contain a JSON object")
    return data


def safe_relative(value: str, field: str) -> PurePosixPath:
    if not isinstance(value, str) or not value.strip():
        fail(f"{field} must be a non-empty relative path")
    normalized = value.replace("\\", "/").strip()
    path = PurePosixPath(normalized)
    if path.is_absolute() or ".." in path.parts or ":" in normalized:
        fail(f"{field} contains an unsafe path: {value!r}")
    if not path.parts:
        fail(f"{field} is empty")
    lower_parts = {part.lower() for part in path.parts}
    if lower_parts & FORBIDDEN_PARTS:
        fail(f"{field} points into a private/forbidden directory: {value!r}")
    name = path.name.lower()
    if name in FORBIDDEN_NAMES or path.suffix.lower() in FORBIDDEN_SUFFIXES:
        fail(f"{field} references a forbidden file type: {value!r}")
    return path


def ensure_file(root: Path, relative: PurePosixPath, field: str, allowed_exts: set[str] | None = None) -> Path:
    path = root.joinpath(*relative.parts)
    if not path.is_file():
        fail(f"{field} references a missing file: {relative.as_posix()}")
    if allowed_exts is not None and path.suffix.lower() not in allowed_exts:
        fail(f"{field} uses unsupported file type {path.suffix!r}: {relative.as_posix()}")
    if path.stat().st_size > MAX_FILE_BYTES:
        fail(f"{field} exceeds the {MAX_FILE_BYTES // (1024 * 1024)} MiB per-file limit")
    return path


def animation_files(root: Path, relative: PurePosixPath, field: str) -> list[Path]:
    directory = root.joinpath(*relative.parts)
    if not directory.is_dir():
        fail(f"{field} references a missing animation directory: {relative.as_posix()}")
    files = sorted(p for p in directory.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_EXTENSIONS)
    if not files:
        fail(f"{field} contains no supported animation frames: {relative.as_posix()}")
    for path in files:
        if path.stat().st_size > MAX_FILE_BYTES:
            fail(f"animation frame is too large: {path.name}")
    return files


def validate_manifest(root: Path) -> tuple[dict, set[Path]]:
    root = root.resolve()
    manifest = load_manifest(root)
    referenced: set[Path] = {root / "pet.json"}

    if manifest.get("schema") != SCHEMA_VERSION:
        fail(f"schema must be {SCHEMA_VERSION}")

    pet_id = manifest.get("id")
    if not isinstance(pet_id, str) or not ID_RE.fullmatch(pet_id):
        fail("id must be 2-64 lowercase letters/numbers plus '.', '_' or '-'")

    name = manifest.get("name")
    if not isinstance(name, str) or not name.strip() or len(name.strip()) > 80:
        fail("name must be a non-empty string no longer than 80 characters")

    version = manifest.get("version")
    if not isinstance(version, str) or not SEMVER_RE.fullmatch(version):
        fail("version must use semantic versioning, for example 1.2.0")

    rights = manifest.get("rights")
    if not isinstance(rights, dict) or rights.get("confirmed") is not True:
        fail("rights.confirmed must be true before a pet can be packaged")

    presentation = manifest.get("presentation", {})
    if not isinstance(presentation, dict):
        fail("presentation must be an object")
    for key in ("default_width", "default_height"):
        if key in presentation:
            value = presentation[key]
            if not isinstance(value, int) or not 48 <= value <= 2048:
                fail(f"presentation.{key} must be an integer between 48 and 2048")

    poses = manifest.get("poses")
    if not isinstance(poses, dict) or not poses:
        fail("poses must contain at least one pose")
    if "idle" not in poses:
        fail("poses.idle is required")

    for pose_name, pose in poses.items():
        if not isinstance(pose_name, str) or not ID_RE.fullmatch(pose_name):
            fail(f"invalid pose name: {pose_name!r}")
        if not isinstance(pose, dict):
            fail(f"poses.{pose_name} must be an object")
        state = pose.get("state")
        animation_dir = pose.get("animation_dir")
        if not state and not animation_dir:
            fail(f"poses.{pose_name} needs state and/or animation_dir")
        if state:
            rel = safe_relative(state, f"poses.{pose_name}.state")
            referenced.add(ensure_file(root, rel, f"poses.{pose_name}.state", IMAGE_EXTENSIONS))
        if animation_dir:
            rel = safe_relative(animation_dir, f"poses.{pose_name}.animation_dir")
            referenced.update(animation_files(root, rel, f"poses.{pose_name}.animation_dir"))
        if "fps" in pose:
            fps = pose["fps"]
            if not isinstance(fps, (int, float)) or not 1 <= float(fps) <= 60:
                fail(f"poses.{pose_name}.fps must be between 1 and 60")
        if "sound" in pose:
            rel = safe_relative(pose["sound"], f"poses.{pose_name}.sound")
            referenced.add(ensure_file(root, rel, f"poses.{pose_name}.sound", AUDIO_EXTENSIONS))
        bubbles = pose.get("bubbles", [])
        if not isinstance(bubbles, list) or any(not isinstance(v, str) or len(v) > 280 for v in bubbles):
            fail(f"poses.{pose_name}.bubbles must be a list of strings up to 280 characters")

    persona = manifest.get("persona", {})
    if persona:
        if not isinstance(persona, dict):
            fail("persona must be an object")
        for key in ("response_pack", "autonomy_pack", "prompt"):
            value = persona.get(key)
            if not value:
                continue
            rel = safe_relative(value, f"persona.{key}")
            referenced.add(ensure_file(root, rel, f"persona.{key}"))

    total = sum(path.stat().st_size for path in referenced)
    if total > MAX_PACKAGE_BYTES:
        fail(f"referenced content exceeds the {MAX_PACKAGE_BYTES // (1024 * 1024)} MiB package limit")

    for path in referenced:
        relative = path.resolve().relative_to(root)
        lowered = {part.lower() for part in relative.parts}
        if lowered & FORBIDDEN_PARTS or path.name.lower() in FORBIDDEN_NAMES or path.suffix.lower() in FORBIDDEN_SUFFIXES:
            fail(f"private or executable content cannot be packaged: {relative.as_posix()}")

    return manifest, referenced


def validate(root: Path) -> None:
    manifest, referenced = validate_manifest(root)
    total = sum(path.stat().st_size for path in referenced)
    print(
        f"TONYPET_VALID=PASS id={manifest['id']} version={manifest['version']} "
        f"files={len(referenced)} bytes={total}"
    )


def pack(root: Path, output: Path) -> None:
    root = root.resolve()
    manifest, referenced = validate_manifest(root)
    output = output.resolve()
    if output.suffix.lower() != ".tonypet":
        fail("output file must use the .tonypet extension")
    output.parent.mkdir(parents=True, exist_ok=True)
    temp = output.with_suffix(output.suffix + ".tmp")
    if temp.exists():
        temp.unlink()
    with zipfile.ZipFile(temp, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(referenced, key=lambda p: p.as_posix()):
            arcname = path.resolve().relative_to(root).as_posix()
            archive.write(path, arcname)
    if temp.stat().st_size > MAX_PACKAGE_BYTES:
        temp.unlink(missing_ok=True)
        fail("compressed package exceeds the maximum package size")
    temp.replace(output)
    print(f"TONYPET_PACK=PASS id={manifest['id']} version={manifest['version']} path={output}")


def unpack(package: Path, destination: Path) -> None:
    package = package.resolve()
    if not package.is_file() or package.suffix.lower() != ".tonypet":
        fail("input must be an existing .tonypet file")
    destination = destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(package, "r") as archive:
        infos = archive.infolist()
        if sum(info.file_size for info in infos) > MAX_PACKAGE_BYTES:
            fail("package expands beyond the maximum allowed size")
        for info in infos:
            rel = safe_relative(info.filename, "archive entry")
            target = destination.joinpath(*rel.parts).resolve()
            if destination != target and destination not in target.parents:
                fail(f"unsafe archive entry: {info.filename!r}")
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            if info.file_size > MAX_FILE_BYTES:
                fail(f"archive entry exceeds per-file limit: {info.filename}")
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(info, "r") as src, target.open("wb") as dst:
                shutil.copyfileobj(src, dst)
    validate(destination)
    print(f"TONYPET_UNPACK=PASS path={destination}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Tony Pet package creator and validator")
    sub = parser.add_subparsers(dest="command", required=True)

    validate_parser = sub.add_parser("validate", help="validate a pet project folder")
    validate_parser.add_argument("root", type=Path)

    pack_parser = sub.add_parser("pack", help="build a .tonypet package")
    pack_parser.add_argument("root", type=Path)
    pack_parser.add_argument("output", type=Path)

    unpack_parser = sub.add_parser("unpack", help="safely unpack and validate a .tonypet package")
    unpack_parser.add_argument("package", type=Path)
    unpack_parser.add_argument("destination", type=Path)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        if args.command == "validate":
            validate(args.root)
        elif args.command == "pack":
            pack(args.root, args.output)
        elif args.command == "unpack":
            unpack(args.package, args.destination)
        return 0
    except (PetPackageError, OSError, zipfile.BadZipFile) as exc:
        print(f"TONYPET_ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
