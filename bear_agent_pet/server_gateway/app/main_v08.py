from __future__ import annotations

import secrets
from typing import Any

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect

from .main import PairRequest, action_for_text, authorized, call_backend_stream, local_model_id, send_json
from .pairing import consume_pairing_code, paired_device_count

app = FastAPI(title="Tony Desktop Companion", version="0.8.4")


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
        "version": "0.8.4",
        "backend": "local-qwen-chat-only",
        "local_model": model,
        "model_profile": "quality" if "2b" in model.casefold() and "0.8b" not in model.casefold() else "fast",
        "language": "English",
        "persona": "Paula-boyfriend",
        "chat_only": True,
        "streaming": True,
        "persona_core": True,
        "pairing_supported": True,
        "paired_devices": paired_device_count(),
        "local_tools_enabled": False,
        "openclaw_enabled": False,
    }


@app.post("/pair")
async def pair_device(request: PairRequest) -> dict[str, Any]:
    try:
        token, device_id = consume_pairing_code(request.code, request.device_name)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    return {
        "ok": True,
        "device_id": device_id,
        "token": token,
        "ws_path": "/agent/ws",
    }


@app.websocket("/agent/ws")
async def agent_ws(ws: WebSocket) -> None:
    if not authorized(ws):
        await ws.close(code=4401, reason="Unauthorized")
        return

    await ws.accept()
    connection_id = secrets.token_hex(6)
    session_key = f"tony-paula-{connection_id}"

    await send_json(ws, {"type": "agent_state", "state": "idle"})
    await send_json(ws, {
        "type": "avatar_action",
        "action": "wave",
        "emotion": "affectionate",
        "duration_ms": 1600,
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
                await send_json(ws, {
                    "type": "client_hello_ack",
                    "protocol_version": "1",
                    "server_version": "0.8.4",
                    "accepted_capabilities": [],
                    "chat_only": True,
                    "streaming": True,
                    "local_model": local_model_id(),
                })
                continue

            if message_type not in {"message", "user_message"}:
                continue

            content = str(payload.get("content", "")).strip()
            if not content:
                continue

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

                answer, used_agent, fallback_used = await call_backend_stream(content, session_key, emit_delta)
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
