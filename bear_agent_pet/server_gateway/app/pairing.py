from __future__ import annotations

import hashlib
import json
import os
import secrets
import time
from pathlib import Path
from threading import Lock
from typing import Any

_LOCK = Lock()
_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"


def _store_path() -> Path:
    return Path(
        os.getenv(
            "BEAR_PAIRING_STORE",
            "/home/ubuntu/.local/share/bear-agent/pairing.json",
        )
    )


def _empty_store() -> dict[str, Any]:
    return {"version": 1, "pairing": None, "devices": []}


def _read_store() -> dict[str, Any]:
    path = _store_path()
    if not path.exists():
        return _empty_store()
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return _empty_store()
    if not isinstance(data, dict):
        return _empty_store()
    data.setdefault("version", 1)
    data.setdefault("pairing", None)
    data.setdefault("devices", [])
    return data


def _write_store(data: dict[str, Any]) -> None:
    path = _store_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        path.parent.chmod(0o700)
    except OSError:
        pass
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    try:
        temp.chmod(0o600)
    except OSError:
        pass
    temp.replace(path)


def _digest(secret: str) -> str:
    return hashlib.sha256(secret.encode("utf-8")).hexdigest()


def _normalize_code(code: str) -> str:
    return "".join(ch for ch in code.upper() if ch.isalnum())


def _new_pairing_code() -> str:
    raw = "".join(secrets.choice(_ALPHABET) for _ in range(12))
    return "-".join(raw[i : i + 4] for i in range(0, 12, 4))


def issue_pairing_code(ttl_seconds: int = 600) -> tuple[str, int]:
    ttl_seconds = max(60, min(int(ttl_seconds), 3600))
    code = _new_pairing_code()
    expires_at = int(time.time()) + ttl_seconds
    with _LOCK:
        data = _read_store()
        data["pairing"] = {
            "code_hash": _digest(_normalize_code(code)),
            "expires_at": expires_at,
            "used": False,
        }
        _write_store(data)
    return code, expires_at


def consume_pairing_code(code: str, device_name: str) -> tuple[str, str]:
    normalized = _normalize_code(code)
    if len(normalized) != 12:
        raise ValueError("配对码格式不正确")
    clean_name = " ".join(device_name.split()).strip()[:80] or "Tony desktop"
    now = int(time.time())

    with _LOCK:
        data = _read_store()
        pairing = data.get("pairing")
        if not isinstance(pairing, dict):
            raise ValueError("当前没有可用的配对码")
        if pairing.get("used"):
            raise ValueError("这个配对码已经使用过")
        if int(pairing.get("expires_at") or 0) < now:
            raise ValueError("配对码已经过期，请重新生成")
        if not secrets.compare_digest(str(pairing.get("code_hash") or ""), _digest(normalized)):
            raise ValueError("配对码不正确")

        token = secrets.token_urlsafe(36)
        device_id = secrets.token_hex(8)
        devices = data.setdefault("devices", [])
        if not isinstance(devices, list):
            devices = []
            data["devices"] = devices
        devices.append(
            {
                "id": device_id,
                "name": clean_name,
                "token_hash": _digest(token),
                "created_at": now,
                "revoked": False,
            }
        )
        pairing["used"] = True
        pairing["used_at"] = now
        pairing["device_id"] = device_id
        _write_store(data)
    return token, device_id


def token_valid(token: str) -> bool:
    if not token:
        return False
    target = _digest(token)
    with _LOCK:
        data = _read_store()
        devices = data.get("devices") or []
        for device in devices:
            if not isinstance(device, dict) or device.get("revoked"):
                continue
            if secrets.compare_digest(str(device.get("token_hash") or ""), target):
                return True
    return False


def list_devices() -> list[dict[str, Any]]:
    with _LOCK:
        data = _read_store()
    rows: list[dict[str, Any]] = []
    for device in data.get("devices") or []:
        if not isinstance(device, dict):
            continue
        rows.append(
            {
                "id": device.get("id"),
                "name": device.get("name"),
                "created_at": device.get("created_at"),
                "revoked": bool(device.get("revoked")),
            }
        )
    return rows


def paired_device_count() -> int:
    return sum(1 for row in list_devices() if not row["revoked"])


def revoke_device(device_id: str) -> bool:
    with _LOCK:
        data = _read_store()
        changed = False
        for device in data.get("devices") or []:
            if isinstance(device, dict) and device.get("id") == device_id and not device.get("revoked"):
                device["revoked"] = True
                device["revoked_at"] = int(time.time())
                changed = True
        if changed:
            _write_store(data)
        return changed
