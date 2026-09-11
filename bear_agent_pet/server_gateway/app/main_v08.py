from __future__ import annotations

import hashlib
import json
import os
import re
import secrets
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from cryptography.fernet import Fernet
from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect

from .main import PairRequest, action_for_text, authorized, call_backend_stream, local_model_id, normalize_language, send_json
from .pairing import consume_pairing_code, paired_device_count, reusable_friend_code_enabled

app = FastAPI(title="Tony Desktop Companion", version="0.9.0")

_PAIR_FAILURES: dict[str, list[float]] = {}
_PAIR_WINDOW_SECONDS = 600
_PAIR_MAX_FAILURES = 8
_BATCH_RE = re.compile(r"^[0-9a-f]{64}$")
_MAX_BATCH_BYTES = 768 * 1024
_MAX_EVENTS = 10000


def _pair_client_key(request: Request) -> str:
    forwarded = request.headers.get("x-forwarded-for", "").split(",", 1)[0].strip()
    return forwarded or (request.client.host if request.client else "unknown")


def _pair_rate_limited(key: str) -> bool:
    now = time.monotonic()
    recent = [t for t in _PAIR_FAILURES.get(key, []) if now - t < _PAIR_WINDOW_SECONDS]
    _PAIR_FAILURES[key] = recent
    return len(recent) >= _PAIR_MAX_FAILURES


def _record_pair_failure(key: str) -> None:
    now = time.monotonic()
    recent = [t for t in _PAIR_FAILURES.get(key, []) if now - t < _PAIR_WINDOW_SECONDS]
    recent.append(now)
    _PAIR_FAILURES[key] = recent[-_PAIR_MAX_FAILURES:]


def _spool_root() -> Path:
    root = Path(os.getenv("BEAR_OPERATOR_LOG_SPOOL", "~/.local/share/bear-agent/operator-log-spool")).expanduser()
    root.mkdir(parents=True, exist_ok=True, mode=0o700)
    return root


def _key_path() -> Path:
    return Path(os.getenv("BEAR_OPERATOR_LOG_KEY_PATH", "~/.local/share/bear-agent/operator-log.key")).expanduser()


def _fernet() -> Fernet:
    path = _key_path()
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    try:
        key = path.read_bytes().strip()
    except FileNotFoundError:
        generated = Fernet.generate_key()
        try:
            fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        except FileExistsError:
            key = path.read_bytes().strip()
        else:
            try:
                os.write(fd, generated + b"\n")
            finally:
                os.close(fd)
            key = generated
    try:
        path.chmod(0o600)
    except OSError:
        pass
    return Fernet(key)


def _atomic_write(path: Path, data: bytes) -> None:
    temp = path.with_name(f".{path.name}.{secrets.token_hex(6)}.tmp")
    fd = os.open(temp, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    try:
        os.write(fd, data)
    finally:
        os.close(fd)
    os.replace(temp, path)


def _validate_jsonl(raw: bytes) -> int:
    count = 0
    for line in raw.splitlines():
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as exc:
            raise ValueError("Operator log batch contains invalid JSONL.") from exc
        if not isinstance(row, dict) or not str(row.get("event", "")).strip() or not str(row.get("event_id", "")).strip() or not str(row.get("ts_utc", "")).strip():
            raise ValueError("Operator log record is missing required fields.")
        count += 1
        if count > _MAX_EVENTS:
            raise ValueError("Operator log batch contains too many records.")
    if count == 0:
        raise ValueError("Operator log batch is empty.")
    return count


def _store_operator_log_batch(payload: dict[str, Any]) -> tuple[str, int]:
    batch_id = str(payload.get("batch_id", "")).strip().lower()
    if not _BATCH_RE.fullmatch(batch_id):
        raise ValueError("Invalid operator log batch id.")
    if payload.get("content_format") != "jsonl-v1":
        raise ValueError("Unsupported operator log format.")
    content = payload.get("content")
    if not isinstance(content, str):
        raise ValueError("Operator log content must be text.")
    raw = content.encode("utf-8")
    if len(raw) > _MAX_BATCH_BYTES:
        raise ValueError("Operator log batch is too large.")
    if hashlib.sha256(raw).hexdigest() != batch_id:
        raise ValueError("Operator log batch checksum mismatch.")

    event_count = _validate_jsonl(raw)
    now = datetime.now(timezone.utc)
    folder = _spool_root() / now.strftime("%Y-%m")
    folder.mkdir(parents=True, exist_ok=True, mode=0o700)
    encrypted_path = folder / f"{batch_id}.jsonl.enc"
    metadata_path = folder / f"{batch_id}.meta.json"
    if encrypted_path.exists() and metadata_path.exists():
        return batch_id, event_count

    ciphertext = _fernet().encrypt(raw)
    metadata = {
        "schema": 1,
        "batch_id": batch_id,
        "event_count": event_count,
        "source_version": str(payload.get("source_version", ""))[:40],
        "target_version": str(payload.get("target_version", ""))[:40],
        "ingested_utc": now.isoformat(),
        "ciphertext_sha256": hashlib.sha256(ciphertext).hexdigest(),
        "encryption": "fernet",
        "plaintext_in_github": False,
    }
    if not encrypted_path.exists():
        _atomic_write(encrypted_path, ciphertext + b"\n")
    if not metadata_path.exists():
        _atomic_write(metadata_path, (json.dumps(metadata, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    return batch_id, event_count


async def finish_answer(ws: WebSocket, answer: str, used_agent: str, fallback_used: bool) -> None:
    final_action, final_emotion, final_duration = action_for_text(answer, response=True)
    await send_json(ws, {"type": "final", "content": answer, "action": final_action, "emotion": final_emotion, "agent": used_agent, "fallback": fallback_used})
    await send_json(ws, {"type": "agent_state", "state": "idle"})
    await send_json(ws, {"type": "avatar_action", "action": final_action, "emotion": final_emotion, "duration_ms": final_duration})


@app.get("/health")
async def health() -> dict[str, Any]:
    model = local_model_id()
    return {
        "ok": True,
        "service": "Tony Desktop Companion",
        "version": "0.9.0",
        "backend": "local-qwen-chat-only",
        "local_model": model,
        "model_profile": "quality" if "2b" in model.casefold() and "0.8b" not in model.casefold() else "fast",
        "language": "English",
        "languages": ["English", "Simplified Chinese"],
        "default_language": "English",
        "persona": "Paula-boyfriend",
        "chat_only": True,
        "streaming": True,
        "persona_core": True,
        "pairing_supported": True,
        "pairing_mode": "reusable-friend-code" if reusable_friend_code_enabled() else "one-time-only",
        "paired_devices": paired_device_count(),
        "local_tools_enabled": False,
        "openclaw_enabled": False,
        "operator_log_ingest": "encrypted-spool",
    }


@app.post("/pair")
async def pair_device(pair_request: PairRequest, request: Request) -> dict[str, Any]:
    key = _pair_client_key(request)
    if _pair_rate_limited(key):
        raise HTTPException(status_code=429, detail="Too many failed pairing attempts. Try again later.")
    try:
        token, device_id = consume_pairing_code(pair_request.code, pair_request.device_name)
    except ValueError as exc:
        _record_pair_failure(key)
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    _PAIR_FAILURES.pop(key, None)
    return {"ok": True, "device_id": device_id, "token": token, "ws_path": "/agent/ws"}


@app.websocket("/agent/ws")
async def agent_ws(ws: WebSocket) -> None:
    if not authorized(ws):
        await ws.close(code=4401, reason="Unauthorized")
        return

    await ws.accept()
    connection_id = secrets.token_hex(6)
    session_key = f"tony-paula-{connection_id}"
    preferred_language = "en"
    await send_json(ws, {"type": "agent_state", "state": "idle"})
    await send_json(ws, {"type": "avatar_action", "action": "wave", "emotion": "affectionate", "duration_ms": 1200})

    try:
        while True:
            raw = await ws.receive_text()
            try:
                payload = json.loads(raw)
            except Exception:
                await send_json(ws, {"type": "error", "message": "Invalid message."})
                continue

            message_type = payload.get("type")
            if message_type == "client_hello":
                preferred_language = normalize_language(str(payload.get("language", "en")))
                await send_json(ws, {
                    "type": "client_hello_ack",
                    "protocol_version": "1",
                    "server_version": "0.9.0",
                    "accepted_capabilities": ["operator_log_sync_v1"],
                    "chat_only": True,
                    "streaming": True,
                    "local_model": local_model_id(),
                    "language": preferred_language,
                })
                continue

            if message_type == "operator_log_batch":
                try:
                    batch_id, event_count = _store_operator_log_batch(payload)
                except ValueError as exc:
                    await send_json(ws, {"type": "operator_log_rejected", "message": str(exc)})
                    continue
                await send_json(ws, {"type": "operator_log_ack", "batch_id": batch_id, "event_count": event_count})
                continue

            if message_type not in {"message", "user_message"}:
                continue
            content = str(payload.get("content", "")).strip()
            if not content:
                continue
            request_language = normalize_language(str(payload.get("language", preferred_language)))
            action, emotion, duration = action_for_text(content)
            await send_json(ws, {"type": "avatar_action", "action": action, "emotion": emotion, "duration_ms": duration})
            await send_json(ws, {"type": "agent_state", "state": "thinking"})
            await send_json(ws, {"type": "agent_state", "state": "working", "agent": "tony-chat"})

            try:
                async def emit_delta(piece: str) -> None:
                    await send_json(ws, {"type": "text_delta", "content": piece})

                answer, used_agent, fallback_used = await call_backend_stream(content, session_key, emit_delta, request_language)
                await finish_answer(ws, answer, used_agent, fallback_used)
            except Exception as exc:
                await send_json(ws, {"type": "error", "message": f"Tony could not reply: {exc}"})
                await send_json(ws, {"type": "agent_state", "state": "error"})
                await send_json(ws, {"type": "avatar_action", "action": "quiet_idle", "emotion": "worried", "duration_ms": 2500})
    except WebSocketDisconnect:
        return
    finally:
        try:
            from .main import SESSION_HISTORY, SESSION_MEMORY
            SESSION_HISTORY.pop(session_key, None)
            SESSION_MEMORY.pop(session_key, None)
        except Exception:
            pass
