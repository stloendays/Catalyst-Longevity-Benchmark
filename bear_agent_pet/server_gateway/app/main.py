import asyncio
import hmac
import json
import os
import secrets
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect

app = FastAPI(title="BearAgentPet Gateway", version="0.3.0")


def env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


def authorized(websocket: WebSocket) -> bool:
    if not env_bool("BEAR_AGENT_REQUIRE_AUTH", True):
        return True
    expected = os.getenv("BEAR_AGENT_TOKEN", "").strip()
    if not expected:
        return False
    header = websocket.headers.get("authorization", "")
    if not header.lower().startswith("bearer "):
        return False
    return hmac.compare_digest(header[7:].strip(), expected)


async def send_json(ws: WebSocket, payload: dict[str, Any]) -> None:
    await ws.send_text(json.dumps(payload, ensure_ascii=False))


def collect_text(value: Any) -> list[str]:
    """Extract human-readable assistant text from varying OpenClaw JSON shapes."""
    found: list[str] = []
    preferred = {"text", "content", "message", "output", "response", "answer"}

    def walk(node: Any, key: str = "") -> None:
        if isinstance(node, str):
            if key.lower() in preferred and node.strip():
                found.append(node.strip())
            return
        if isinstance(node, list):
            for item in node:
                walk(item, key)
            return
        if isinstance(node, dict):
            for k, v in node.items():
                walk(v, str(k))

    walk(value)
    # Keep order while removing duplicates.
    return list(dict.fromkeys(found))


async def call_openclaw(message: str, session_key: str) -> str:
    oc = os.getenv("OPENCLAW_BIN", "/home/ubuntu/.npm-global/bin/openclaw")
    agent = os.getenv("BEAR_OPENCLAW_AGENT", "main")
    timeout_seconds = int(os.getenv("BEAR_OPENCLAW_TIMEOUT", "240"))

    proc = await asyncio.create_subprocess_exec(
        oc,
        "agent",
        "--agent",
        agent,
        "--session-key",
        session_key,
        "--message",
        message,
        "--timeout",
        str(timeout_seconds),
        "--json",
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        env={**os.environ, "PATH": f"/home/ubuntu/.npm-global/bin:{os.environ.get('PATH', '')}"},
    )
    try:
        stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=timeout_seconds + 20)
    except asyncio.TimeoutError:
        proc.kill()
        await proc.wait()
        raise RuntimeError("OpenClaw request timed out")

    out = stdout.decode("utf-8", errors="replace").strip()
    err = stderr.decode("utf-8", errors="replace").strip()
    if proc.returncode != 0:
        raise RuntimeError(err[-1500:] or out[-1500:] or f"OpenClaw exited {proc.returncode}")
    if not out:
        raise RuntimeError("OpenClaw returned empty output")

    try:
        payload = json.loads(out)
        texts = collect_text(payload)
        if texts:
            return "\n".join(texts)
    except json.JSONDecodeError:
        pass
    return out


@app.get("/health")
async def health() -> dict[str, Any]:
    return {
        "ok": True,
        "service": "BearAgentPet Gateway",
        "backend": "openclaw-cli",
        "agent": os.getenv("BEAR_OPENCLAW_AGENT", "main"),
        "auth_required": env_bool("BEAR_AGENT_REQUIRE_AUTH", True),
    }


@app.websocket("/agent/ws")
async def agent_ws(ws: WebSocket) -> None:
    if not authorized(ws):
        await ws.close(code=4401, reason="Unauthorized")
        return

    await ws.accept()
    connection_id = secrets.token_hex(6)
    session_key = f"bearpet-{connection_id}"
    await send_json(ws, {"type": "agent_state", "state": "idle"})

    try:
        while True:
            raw = await ws.receive_text()
            try:
                payload = json.loads(raw)
            except json.JSONDecodeError:
                await send_json(ws, {"type": "error", "message": "消息不是合法 JSON"})
                continue

            if payload.get("type") != "message":
                await send_json(ws, {"type": "error", "message": "不支持的消息类型"})
                continue
            content = str(payload.get("content", "")).strip()
            if not content:
                continue

            await send_json(ws, {"type": "agent_state", "state": "thinking"})
            try:
                answer = await call_openclaw(content, session_key)
                await send_json(ws, {"type": "agent_state", "state": "working"})
                # Chunk the final answer so Qt can render progressively even though the CLI is turn-based.
                for i in range(0, len(answer), 24):
                    await send_json(ws, {"type": "text_delta", "content": answer[i:i + 24]})
                    await asyncio.sleep(0.005)
                await send_json(ws, {"type": "final", "content": answer})
                await send_json(ws, {"type": "agent_state", "state": "idle"})
            except Exception as exc:
                await send_json(ws, {"type": "error", "message": f"OpenClaw 调用失败：{exc}"})
                await send_json(ws, {"type": "agent_state", "state": "error"})
    except WebSocketDisconnect:
        return
