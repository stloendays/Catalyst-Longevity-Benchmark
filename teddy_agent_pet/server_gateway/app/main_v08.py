from __future__ import annotations

import secrets
import time
from typing import Any

from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from pydantic import BaseModel, Field

from .main import PairRequest, action_for_text, authorized, call_backend_stream, local_model_id, normalize_language, send_json
from .pairing import consume_pairing_code, issue_trusted_device, paired_device_count, reusable_friend_code_enabled

app = FastAPI(title="Tony Desktop Companion", version="0.8.5")

_PAIR_FAILURES: dict[str, list[float]] = {}
_PAIR_WINDOW_SECONDS = 600
_PAIR_MAX_FAILURES = 8


class SshBootstrapRequest(BaseModel):
    device_name: str = Field(default="Tony desktop", min_length=1, max_length=80)


def _pair_client_key(request: Request) -> str:
    forwarded = request.headers.get("x-forwarded-for", "").split(",", 1)[0].strip()
    if forwarded:
        return forwarded
    return request.client.host if request.client else "unknown"


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


def _direct_loopback_request(request: Request) -> bool:
    """Accept only a direct call to the loopback-bound gateway, never reverse-proxied traffic."""
    remote = (request.client.host if request.client else "").strip().casefold()
    if remote not in {"127.0.0.1", "::1"}:
        return False

    host_header = request.headers.get("host", "").strip().casefold()
    if host_header.startswith("[") and "]" in host_header:
        host_only = host_header.split("]", 1)[0] + "]"
    else:
        host_only = host_header.split(":", 1)[0]
    if host_only not in {"127.0.0.1", "localhost", "[::1]"}:
        return False

    proxy_headers = (
        "forwarded",
        "via",
        "x-forwarded-for",
        "x-forwarded-host",
        "x-forwarded-proto",
        "x-real-ip",
    )
    return not any(request.headers.get(name) for name in proxy_headers)


async def finish_answer(ws: WebSocket, answer: str, used_agent: str, fallback_used: bool) -> None:
    final_action, final_emotion, final_duration = action_for_text(answer, response=True)
    await send_json(ws, {
        "type": "final",
        "content": answer,
        "action": final_action,
        "emotion": final_emotion,
        "agent": used_agent,
        "fallback": fallback_used,
    })
    await send_json(ws, {"type": "agent_state", "state": "idle"})
    await send_json(ws, {
        "type": "avatar_action",
        "action": final_action,
        "emotion": final_emotion,
        "duration_ms": final_duration,
    })


@app.get("/health")
async def health() -> dict[str, Any]:
    model = local_model_id()
    return {
        "ok": True,
        "service": "Tony Desktop Companion",
        "version": "0.8.5",
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
        "ssh_bootstrap_supported": True,
        "paired_devices": paired_device_count(),
        "local_tools_enabled": False,
        "openclaw_enabled": False,
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
    return {
        "ok": True,
        "device_id": device_id,
        "token": token,
        "ws_path": "/agent/ws",
    }


@app.post("/pair/ssh-bootstrap")
async def pair_device_via_ssh(pair_request: SshBootstrapRequest, request: Request) -> dict[str, Any]:
    if not _direct_loopback_request(request):
        raise HTTPException(status_code=403, detail="SSH bootstrap is available only through a direct loopback tunnel.")
    token, device_id = issue_trusted_device(pair_request.device_name, "ssh_bootstrap")
    return {
        "ok": True,
        "device_id": device_id,
        "token": token,
        "ws_path": "/agent/ws",
        "paired_via": "ssh_bootstrap",
    }


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
    await send_json(ws, {
        "type": "avatar_action",
        "action": "wave",
        "emotion": "affectionate",
        "duration_ms": 1200,
    })

    try:
        while True:
            raw = await ws.receive_text()
            try:
                import json
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
                    "server_version": "0.8.5",
                    "accepted_capabilities": [],
                    "chat_only": True,
                    "streaming": True,
                    "local_model": local_model_id(),
                    "language": preferred_language,
                })
                continue

            if message_type not in {"message", "user_message"}:
                continue

            content = str(payload.get("content", "")).strip()
            if not content:
                continue
            request_language = normalize_language(str(payload.get("language", preferred_language)))

            action, emotion, duration = action_for_text(content)
            await send_json(ws, {
                "type": "avatar_action",
                "action": action,
                "emotion": emotion,
                "duration_ms": duration,
            })
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
                await send_json(ws, {
                    "type": "avatar_action",
                    "action": "quiet_idle",
                    "emotion": "worried",
                    "duration_ms": 2500,
                })
    except WebSocketDisconnect:
        return
    finally:
        try:
            from .main import SESSION_HISTORY, SESSION_MEMORY
            SESSION_HISTORY.pop(session_key, None)
            SESSION_MEMORY.pop(session_key, None)
        except Exception:
            pass
