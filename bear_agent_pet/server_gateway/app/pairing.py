from __future__ import annotations

import base64
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
_DEFAULT_FRIEND_CODE_SHA256 = "cd63ec788c84ed0b945f7f4a471271c82471347dc0dfeca51ae96e5221a9bedc"


def _store_path() -> Path:
    return Path(
        os.getenv(
            "BEAR_PAIRING_STORE",
            "/home/ubuntu/.local/share/bear-agent/pairing.json",
        )
    )


def _empty_store() -> dict[str, Any]:
    return {"version": 3, "pairing": None, "device_requests": [], "devices": []}


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
    data["version"] = max(3, int(data.get("version") or 0))
    data.setdefault("pairing", None)
    data.setdefault("device_requests", [])
    data.setdefault("devices", [])
    return data


def _write_store(data: dict[str, Any]) -> None:
    path = _store_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        path.parent.chmod(0o700)
    except OSError:
        pass

    # pairing_cli is often run from an administrative root shell while the gateway
    # itself runs as the owner of the pairing-store directory (normally ubuntu).
    # Preserve that service ownership on atomic replacement so root approval cannot
    # turn pairing.json into root:root 0600 and make every saved device disappear
    # from the gateway's point of view.
    try:
        parent_stat = path.parent.stat()
    except OSError:
        parent_stat = None

    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    try:
        temp.chmod(0o600)
    except OSError:
        pass
    if parent_stat is not None and hasattr(os, "geteuid") and os.geteuid() == 0:
        try:
            os.chown(temp, parent_stat.st_uid, parent_stat.st_gid)
        except OSError:
            pass
    temp.replace(path)


def _digest(secret: str) -> str:
    return hashlib.sha256(secret.encode("utf-8")).hexdigest()


def _normalize_code(code: str) -> str:
    return "".join(ch for ch in code.upper() if ch.isalnum())


def _clean_device_name(device_name: str) -> str:
    return " ".join(device_name.split()).strip()[:80] or "Tony desktop"


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


def _new_device_code() -> str:
    raw = "".join(secrets.choice(_ALPHABET) for _ in range(8))
    return f"{raw[:4]}-{raw[4:]}"


def _device_token_for_request(request_id: str) -> str:
    # request_id is an unguessable transaction secret held only by the client. Deriving
    # the device token from it lets an approved client safely retry a lost status reply
    # without ever storing the plaintext device token on the server.
    raw = hashlib.sha256(("tony-device-token-v1:" + request_id).encode("utf-8")).digest()
    return base64.urlsafe_b64encode(raw).decode("ascii").rstrip("=")


def _prune_device_requests(data: dict[str, Any], now: int) -> list[dict[str, Any]]:
    rows = data.get("device_requests")
    if not isinstance(rows, list):
        rows = []
    kept: list[dict[str, Any]] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        expires_at = int(row.get("expires_at") or 0)
        # Keep a short post-expiry audit tail, then remove stale requests automatically.
        if expires_at + 3600 < now:
            continue
        kept.append(row)
    data["device_requests"] = kept[-100:]
    return data["device_requests"]


def create_device_pairing_request(device_name: str, ttl_seconds: int = 259200) -> tuple[str, str, int]:
    """Create an approval-gated code that the desktop can display automatically."""
    # Desktop connection codes remain valid for up to 72 hours so an owner can approve
    # a remote computer without racing a short 10-minute window.
    ttl_seconds = max(120, min(int(ttl_seconds), 259200))
    now = int(time.time())
    expires_at = now + ttl_seconds
    request_id = secrets.token_urlsafe(24)

    with _LOCK:
        data = _read_store()
        requests = _prune_device_requests(data, now)
        existing_hashes = {str(row.get("code_hash") or "") for row in requests}
        code = _new_device_code()
        while _digest(_normalize_code(code)) in existing_hashes:
            code = _new_device_code()
        requests.append(
            {
                "request_hash": _digest(request_id),
                "code_hash": _digest(_normalize_code(code)),
                "device_name": _clean_device_name(device_name),
                "created_at": now,
                "expires_at": expires_at,
                "approved": False,
            }
        )
        _write_store(data)
    return request_id, code, expires_at


def approve_device_pairing_request(code: str) -> dict[str, Any]:
    """Approve a desktop-displayed code. This is intended for local/admin CLI use."""
    normalized = _normalize_code(code)
    if len(normalized) != 8:
        raise ValueError("Device connection code format is invalid")
    code_hash = _digest(normalized)
    now = int(time.time())

    with _LOCK:
        data = _read_store()
        requests = _prune_device_requests(data, now)
        for row in reversed(requests):
            if not secrets.compare_digest(str(row.get("code_hash") or ""), code_hash):
                continue
            if int(row.get("expires_at") or 0) < now:
                raise ValueError("This device connection code has expired")
            row["approved"] = True
            row["approved_at"] = now
            _write_store(data)
            return {
                "device_name": row.get("device_name") or "Tony desktop",
                "expires_at": int(row.get("expires_at") or 0),
                "already_paired": bool(row.get("device_id")),
            }
    raise ValueError("Device connection code is incorrect")


def claim_device_pairing_request(request_id: str) -> tuple[str, str | None, str | None]:
    """Return pending/approved/expired. Approved replies are safely retryable until expiry."""
    request_id = request_id.strip()
    if len(request_id) < 24:
        raise ValueError("Pairing request is invalid")
    target = _digest(request_id)
    now = int(time.time())

    with _LOCK:
        data = _read_store()
        requests = _prune_device_requests(data, now)
        for row in requests:
            if not secrets.compare_digest(str(row.get("request_hash") or ""), target):
                continue
            if int(row.get("expires_at") or 0) < now:
                return "expired", None, None
            if not bool(row.get("approved")):
                return "pending", None, None

            token = _device_token_for_request(request_id)
            device_id = str(row.get("device_id") or "")
            if not device_id:
                _, device_id = _append_device_with_token(
                    data,
                    _clean_device_name(str(row.get("device_name") or "Tony desktop")),
                    now,
                    "device_code",
                    token,
                )
                row["device_id"] = device_id
                row["claimed_at"] = now
                _write_store(data)
            return "approved", token, device_id
    raise ValueError("Pairing request was not found")


def list_device_pairing_requests() -> list[dict[str, Any]]:
    now = int(time.time())
    with _LOCK:
        data = _read_store()
        requests = _prune_device_requests(data, now)
        rows = [
            {
                "device_name": row.get("device_name"),
                "created_at": row.get("created_at"),
                "expires_at": row.get("expires_at"),
                "approved": bool(row.get("approved")),
                "paired": bool(row.get("device_id")),
            }
            for row in requests
            if int(row.get("expires_at") or 0) >= now
        ]
    return rows


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


def _append_device_with_token(
    data: dict[str, Any], clean_name: str, now: int, paired_via: str, token: str
) -> tuple[str, str]:
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


def _append_device(data: dict[str, Any], clean_name: str, now: int, paired_via: str) -> tuple[str, str]:
    token = secrets.token_urlsafe(36)
    return _append_device_with_token(data, clean_name, now, paired_via, token)


def consume_pairing_code(code: str, device_name: str) -> tuple[str, str]:
    normalized = _normalize_code(code)
    if len(normalized) != 12:
        raise ValueError("Pairing code format is invalid")
    clean_name = _clean_device_name(device_name)
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


def _ensure_owner_device_id(data: dict[str, Any]) -> str:
    """Persist the earliest non-revoked paired device as the owner on first use.

    Existing installations predate owner roles. The one-time migration deliberately
    ignores revoked CI/test devices and then freezes the selected id in the pairing store.
    """
    current = str(data.get("owner_device_id") or "").strip()
    if current:
        return current
    active = [
        device for device in (data.get("devices") or [])
        if isinstance(device, dict) and not device.get("revoked") and str(device.get("id") or "").strip()
    ]
    if not active:
        return ""
    active.sort(key=lambda device: int(device.get("created_at") or 0))
    owner_id = str(active[0].get("id") or "").strip()
    if owner_id:
        data["owner_device_id"] = owner_id
    return owner_id


def owner_device_for_token(token: str) -> dict[str, Any] | None:
    """Return safe owner-device metadata when token belongs to the frozen owner device."""
    if not token:
        return None
    target = _digest(token)
    with _LOCK:
        data = _read_store()
        had_owner = bool(str(data.get("owner_device_id") or "").strip())
        owner_id = _ensure_owner_device_id(data)
        if owner_id and not had_owner:
            _write_store(data)
        for device in data.get("devices") or []:
            if not isinstance(device, dict) or device.get("revoked"):
                continue
            if str(device.get("id") or "") != owner_id:
                continue
            if secrets.compare_digest(str(device.get("token_hash") or ""), target):
                return {
                    "id": device.get("id"),
                    "name": device.get("name"),
                    "created_at": device.get("created_at"),
                    "paired_via": device.get("paired_via", "legacy"),
                }
    return None


def token_is_owner(token: str) -> bool:
    return owner_device_for_token(token) is not None


def list_owner_pairing_requests() -> list[dict[str, Any]]:
    """List live pairing requests with opaque approval ids for the owner UI."""
    now = int(time.time())
    with _LOCK:
        data = _read_store()
        requests = _prune_device_requests(data, now)
        changed = False
        rows: list[dict[str, Any]] = []
        for row in requests:
            if int(row.get("expires_at") or 0) < now:
                continue
            approval_id = str(row.get("approval_id") or "").strip()
            if not approval_id:
                approval_id = secrets.token_urlsafe(12)
                row["approval_id"] = approval_id
                changed = True
            rows.append(
                {
                    "approval_id": approval_id,
                    "device_name": row.get("device_name") or "Tony desktop",
                    "created_at": int(row.get("created_at") or 0),
                    "expires_at": int(row.get("expires_at") or 0),
                    "approved": bool(row.get("approved")),
                    "paired": bool(row.get("device_id")),
                }
            )
        if changed:
            _write_store(data)
    rows.sort(key=lambda row: int(row.get("created_at") or 0), reverse=True)
    return rows


def approve_device_pairing_request_by_id(approval_id: str) -> dict[str, Any]:
    """Approve a request selected from the owner UI without exposing its connection code."""
    approval_id = approval_id.strip()
    if len(approval_id) < 8 or len(approval_id) > 80:
        raise ValueError("Pairing approval id is invalid")
    now = int(time.time())
    with _LOCK:
        data = _read_store()
        requests = _prune_device_requests(data, now)
        for row in requests:
            if not secrets.compare_digest(str(row.get("approval_id") or ""), approval_id):
                continue
            if int(row.get("expires_at") or 0) < now:
                raise ValueError("This device connection request has expired")
            row["approved"] = True
            row["approved_at"] = now
            _write_store(data)
            return {
                "device_name": row.get("device_name") or "Tony desktop",
                "expires_at": int(row.get("expires_at") or 0),
                "already_paired": bool(row.get("device_id")),
            }
    raise ValueError("Pairing approval request was not found")


def list_owner_devices() -> list[dict[str, Any]]:
    with _LOCK:
        data = _read_store()
        owner_id = _ensure_owner_device_id(data)
        if owner_id and not data.get("owner_device_id"):
            data["owner_device_id"] = owner_id
            _write_store(data)
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
                    "owner": str(device.get("id") or "") == owner_id,
                }
            )
    rows.sort(key=lambda row: int(row.get("created_at") or 0), reverse=True)
    return rows


def revoke_device_as_owner(device_id: str) -> bool:
    """Revoke a non-owner device. The owner cannot revoke itself through the public API."""
    device_id = device_id.strip()
    if not device_id:
        return False
    with _LOCK:
        data = _read_store()
        owner_id = _ensure_owner_device_id(data)
        if device_id == owner_id:
            raise ValueError("The owner device cannot revoke itself")
        changed = False
        for device in data.get("devices") or []:
            if isinstance(device, dict) and str(device.get("id") or "") == device_id and not device.get("revoked"):
                device["revoked"] = True
                device["revoked_at"] = int(time.time())
                changed = True
                break
        if changed:
            _write_store(data)
        return changed
