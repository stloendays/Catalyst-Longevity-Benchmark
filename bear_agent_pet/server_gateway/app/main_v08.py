from __future__ import annotations

import asyncio
import json
import os
import secrets
from typing import Any

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect

from .local_tools import direct_success_text, looks_chinese, plan_local_tool, tool_context_for_model
from .main import PairRequest, action_for_text, authorized, call_backend, env_bool, send_json
from .pairing import consume_pairing_code, paired_device_count

app = FastAPI(title="Tony Desktop Agent Gateway", version="0.8.0")

LOCAL_TOOL_ALLOWLIST = {
    "read_clipboard",
    "write_clipboard",
    "capture_screen",
    "open_url",
    "open_file",
    "show_notification",
}


async def receive_tool_result(ws: WebSocket, request_id: str, timeout: float = 45.0) -> dict[str, Any]:
    async def wait() -> dict[str, Any]:
        while True:
            raw = await ws.receive_text()
            try:
                payload = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if payload.get("type") == "client_hello":
                # A reconnecting/newer client may repeat hello; ignore it during the in-flight action.
                continue
            if payload.get("type") != "tool_result":
                continue
            if str(payload.get("request_id", "")) != request_id:
                continue
            return payload

    return await asyncio.wait_for(wait(), timeout=timeout)


async def stream_answer(ws: WebSocket, answer: str, used_agent: str, fallback_used: bool) -> None:
    await send_json(ws, {
        "type": "agent_state",
        "state": "working",
        "agent": used_agent,
        "fallback": fallback_used,
    })
    for i in range(0, len(answer), 24):
        await send_json(ws, {"type": "text_delta", "content": answer[i:i + 24]})
        await asyncio.sleep(0.005)
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
    return {
        "ok": True,
        "service": "Tony Desktop Agent Gateway",
        "version": "0.8.0",
        "backend": "hybrid-openclaw-plus-local-qwen",
        "technical_agent": os.getenv("BEAR_OPENCLAW_AGENT", "main"),
        "local_model": os.getenv("BEAR_LOCAL_MODEL", "qwen3.5-2b-q4"),
        "tony_persona": env_bool("BEAR_TONY_PERSONA", True),
        "auth_required": env_bool("BEAR_AGENT_REQUIRE_AUTH", True),
        "pairing_supported": True,
        "paired_devices": paired_device_count(),
        "local_tool_protocol": "1",
        "local_tools": sorted(LOCAL_TOOL_ALLOWLIST),
        "arbitrary_shell": False,
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
    session_key = f"tony-pet-{connection_id}"
    client_capabilities: set[str] = set()

    await send_json(ws, {"type": "agent_state", "state": "idle"})
    await send_json(ws, {"type": "avatar_action", "action": "wave", "emotion": "friendly", "duration_ms": 1600})

    try:
        while True:
            raw = await ws.receive_text()
            try:
                payload = json.loads(raw)
            except json.JSONDecodeError:
                await send_json(ws, {"type": "error", "message": "消息不是合法 JSON"})
                continue

            message_type = payload.get("type")
            if message_type == "client_hello":
                advertised = payload.get("capabilities") or []
                if isinstance(advertised, list):
                    client_capabilities = {
                        str(item) for item in advertised
                        if str(item) in LOCAL_TOOL_ALLOWLIST
                    }
                await send_json(ws, {
                    "type": "client_hello_ack",
                    "protocol_version": "1",
                    "server_version": "0.8.0",
                    "accepted_capabilities": sorted(client_capabilities),
                })
                continue

            if message_type not in {"message", "user_message"}:
                await send_json(ws, {"type": "error", "message": "不支持的消息类型"})
                continue

            content = str(payload.get("content", "")).strip()
            if not content:
                continue

            action, emotion, duration = action_for_text(content)
            await send_json(ws, {"type": "avatar_action", "action": action, "emotion": emotion, "duration_ms": duration})
            await send_json(ws, {"type": "agent_state", "state": "thinking"})

            try:
                plan = plan_local_tool(content)
                if plan is not None:
                    tool = str(plan["tool"])
                    if tool not in LOCAL_TOOL_ALLOWLIST:
                        raise RuntimeError("Local tool planner produced a non-allowlisted tool")
                    if tool not in client_capabilities:
                        answer = (
                            "这台 Tony 客户端还没有声明这个本地工具能力。请更新桌宠或检查“允许 Tony 使用本地工具”。"
                            if looks_chinese(content)
                            else "This Tony client has not advertised that local tool capability. Update the desktop pet or check Local Tools."
                        )
                        await stream_answer(ws, answer, "local-bridge", False)
                        continue

                    request_id = secrets.token_hex(12)
                    await send_json(ws, {"type": "agent_state", "state": "tool_running", "tool": tool})
                    await send_json(ws, {
                        "type": "tool_request",
                        "request_id": request_id,
                        "tool": tool,
                        "args": plan.get("args") or {},
                        "reason": plan.get("reason") or "User requested local action",
                    })

                    try:
                        tool_result = await receive_tool_result(ws, request_id)
                    except asyncio.TimeoutError as exc:
                        raise RuntimeError(f"Local tool {tool} timed out waiting for desktop approval/result") from exc

                    if not bool(tool_result.get("ok")):
                        error = str(tool_result.get("error") or "Local action was not approved or failed")
                        answer = (
                            f"这个本地操作没有执行：{error}"
                            if looks_chinese(content)
                            else f"The local action was not executed: {error}"
                        )
                        await stream_answer(ws, answer, "local-bridge", False)
                        continue

                    result = tool_result.get("result")
                    if not isinstance(result, dict):
                        result = {}

                    if bool(plan.get("needs_backend")):
                        augmented = tool_context_for_model(content, tool, result)
                        answer, used_agent, fallback_used = await call_backend(augmented, session_key)
                    else:
                        answer = direct_success_text(tool, result, chinese=looks_chinese(content))
                        used_agent, fallback_used = "local-bridge", False
                    await stream_answer(ws, answer, used_agent, fallback_used)
                    continue

                answer, used_agent, fallback_used = await call_backend(content, session_key)
                await stream_answer(ws, answer, used_agent, fallback_used)

            except Exception as exc:
                await send_json(ws, {"type": "error", "message": f"Tony 后端调用失败：{exc}"})
                await send_json(ws, {"type": "agent_state", "state": "error"})
                await send_json(ws, {"type": "avatar_action", "action": "quiet_idle", "emotion": "worried", "duration_ms": 2500})
    except WebSocketDisconnect:
        return
