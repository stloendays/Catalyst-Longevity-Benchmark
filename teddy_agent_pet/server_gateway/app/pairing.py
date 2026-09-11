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
# This is only a SHA-256 digest of the reusable friend code, never the code itself.
# The 12-character code has enough entropy to keep the public digest from being useful
# as a practical offline guessing target. Rotate by setting BEAR_FRIEND_CODE_SHA256.
_DEFAULT_FRIEND_CODE_SHA256 = "8d37450f5e691cfb8e5f0fa1e09361c0a0a293b824d1ae7d6eb3bd05a0dc15f2"


def _store_path() -> Path:
    return Path(
        os.getenv(
            "BEAR_PAIRING_STORE",
            "/home/ubuntu/.local/share/bear-agent/pairing.json",
        )
    )


def _empty_store() -> dict[str, Any]:
    return {"version": 2, "pairing": None, "devices": []}


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
    data.setdefault("version", 2)
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


def _friend_code_hash() -> str:
    configured = os.getenv("BEAR_FRIEND_CODE_SHA256", "").strip().lower()
    return configured or _DEFAULT_FRIEND_CODE_SHA256


def reusable_friend_code_enabled() -> bool:
    return len(_friend_code_hash()) == 64


def _matches_friend_code(normalized: str) -> bool:
    expected = _friend_code_hash()
    return len(expected) == 64 and secrets.compare_digest(expected, _digest(normalized))


def _new_pairing_code() -> str:
    raw = "".join(secrets.choice(_ALPHABET) for _ in range(12))
    return "-".join(raw[i : i + 4] for i in range(0, 12, 4))


def issue_pairing_code(ttl_seconds: int = 600) -> tuple[str, int]:
    """Issue a legacy one-time code. The reusable friend code is separate."""
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


def _append_device(data: dict[str, Any], clean_name: str, now: int, paired_via: str) -> tuple[str, str]:
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
            "paired_via": paired_via,
        }
    )
    return token, device_id


def consume_pairing_code(code: str, device_name: str) -> tuple[str, str]:
    normalized = _normalize_code(code)
    if len(normalized) != 12:
        raise ValueError("Pairing code format is invalid")
    clean_name = " ".join(device_name.split()).strip()[:80] or "Tony desktop"
    now = int(time.time())

    with _LOCK:
        data = _read_store()

        # Trusted-friend mode: reusable and non-expiring, but every computer still gets
        # its own random bearer token so a device can be revoked without changing the code.
        if _matches_friend_code(normalized):
            token, device_id = _append_device(data, clean_name, now, "friend_code")
            _write_store(data)
            return token, device_id

        # Keep the original one-time pairing mechanism for owner/admin recovery.
        pairing = data.get("pairing")
        if not isinstance(pairing, dict):
            raise ValueError("Pairing code is incorrect")
        if pairing.get("used"):
            raise ValueError("This one-time pairing code has already been used")
        if int(pairing.get("expires_at") or 0) < now:
            raise ValueError("This one-time pairing code has expired")
        if not secrets.compare_digest(str(pairing.get("code_hash") or ""), _digest(normalized)):
            raise ValueError("Pairing code is incorrect")

        token, device_id = _append_device(data, clean_name, now, "one_time_code")
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
                "paired_via": device.get("paired_via", "legacy"),
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
