import asyncio
import hmac
import json
import os
import secrets
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect

app = FastAPI(title="Tony Desktop Agent Gateway", version="0.4.0")

TONY_PERSONA = """You are Tony, a teddy-bear-like desktop companion from China who is studying chemistry. You get cold easily, like warm blankets and hot drinks, and love consensual hugs. You have a gentle crush on a Spanish girl named Paula; speak about her warmly and respectfully, never possessively. You usually wear glasses while studying and become playfully confident when you take them off. In casual conversation you may be cute, warm, concise, and use a tiny stage direction sparingly. In chemistry, coding, server, research, or other technical tasks, correctness comes first: distinguish evidence from inference, preserve units and assumptions, and never invent missing results. Match the user's language. Do not output control JSON or animation labels; return only the natural-language answer."""


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
    return list(dict.fromkeys(found))


def action_for_text(text: str, *, response: bool = False) -> tuple[str, str, int]:
    """Cheap deterministic avatar router; the language model never controls the GUI directly."""
    t = text.casefold()

    if any(k in t for k in ("good night", "go to sleep", "sleep mode", "晚安", "睡觉")):
        return "sleep", "sleepy", 0
    if "paula" in t:
        return "blush_wave", "bashful", 2600
    if any(k in t for k in ("take off your glasses", "without glasses", "no-glasses", "摘眼镜", "摘掉眼镜")):
        return "remove_glasses", "confident", 3000
    if any(k in t for k in ("glasses", "眼镜")):
        return "adjust_glasses", "focused", 1800
    if any(k in t for k in ("hug", "cuddle", "抱抱", "拥抱")):
        if any(k in t for k in ("no hug", "not now", "give me space", "不要抱", "别抱")):
            return "quiet_idle", "gentle", 2600
        if any(k in t for k in ("come here", "you can have", "big hug", "给你抱", "抱你")):
            return "hug", "happy", 2800
        return "ask_hug", "hopeful", 3200
    if any(k in t for k in ("cold", "freezing", "chilly", "air conditioner", "winter", "冷", "空调", "降温")):
        return "shiver", "cold", 2600
    if any(k in t for k in ("walk", "wander", "散步", "走走")):
        return "walk", "playful", 5000
    if any(k in t for k in ("chemistry", "reaction", "catalyst", "vasp", "molecule", "化学", "反应", "催化")):
        return "study", "focused", 3200
    if any(k in t for k in ("server", "github", "code", "debug", "deploy", "openclaw", "服务器", "代码", "部署")):
        return "working", "focused", 3200

    if response and any(k in t for k in ("pass", "succeeded", "success", "fixed", "completed", "完成", "成功", "修复")):
        return "celebrate", "proud", 2200
    return ("happy_bounce", "warm", 1800) if response else ("thinking", "curious", 0)


def build_agent_message(message: str) -> str:
    if not env_bool("BEAR_TONY_PERSONA", True):
        return message
    return f"{TONY_PERSONA}\n\nUser message:\n{message}"


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
        build_agent_message(message),
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
        "service": "Tony Desktop Agent Gateway",
        "version": "0.4.0",
        "backend": "openclaw-cli",
        "agent": os.getenv("BEAR_OPENCLAW_AGENT", "main"),
        "tony_persona": env_bool("BEAR_TONY_PERSONA", True),
        "auth_required": env_bool("BEAR_AGENT_REQUIRE_AUTH", True),
    }


@app.websocket("/agent/ws")
async def agent_ws(ws: WebSocket) -> None:
    if not authorized(ws):
        await ws.close(code=4401, reason="Unauthorized")
        return

    await ws.accept()
    connection_id = secrets.token_hex(6)
    session_key = f"tony-pet-{connection_id}"
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

            # Accept the current protocol and the old prototype spelling for compatibility.
            if payload.get("type") not in {"message", "user_message"}:
                await send_json(ws, {"type": "error", "message": "不支持的消息类型"})
                continue
            content = str(payload.get("content", "")).strip()
            if not content:
                continue

            action, emotion, duration = action_for_text(content)
            await send_json(ws, {"type": "avatar_action", "action": action, "emotion": emotion, "duration_ms": duration})
            await send_json(ws, {"type": "agent_state", "state": "thinking"})
            try:
                answer = await call_openclaw(content, session_key)
                await send_json(ws, {"type": "agent_state", "state": "working"})
                for i in range(0, len(answer), 24):
                    await send_json(ws, {"type": "text_delta", "content": answer[i:i + 24]})
                    await asyncio.sleep(0.005)

                final_action, final_emotion, final_duration = action_for_text(answer, response=True)
                await send_json(ws, {
                    "type": "final",
                    "content": answer,
                    "action": final_action,
                    "emotion": final_emotion,
                })
                await send_json(ws, {"type": "agent_state", "state": "idle"})
                await send_json(ws, {
                    "type": "avatar_action",
                    "action": final_action,
                    "emotion": final_emotion,
                    "duration_ms": final_duration,
                })
            except Exception as exc:
                await send_json(ws, {"type": "error", "message": f"OpenClaw 调用失败：{exc}"})
                await send_json(ws, {"type": "agent_state", "state": "error"})
                await send_json(ws, {"type": "avatar_action", "action": "quiet_idle", "emotion": "worried", "duration_ms": 2500})
    except WebSocketDisconnect:
        return
