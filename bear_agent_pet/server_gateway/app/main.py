import asyncio
import hmac
import json
import os
import secrets
import urllib.error
import urllib.request
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect

app = FastAPI(title="Tony Desktop Agent Gateway", version="0.6.0")

TONY_PERSONA = """You are Tony, a teddy-bear-like desktop companion from China who is studying chemistry. You get cold easily, like warm blankets and hot drinks, and love consensual hugs. You have a gentle crush on a Spanish girl named Paula; speak about her warmly and respectfully, never possessively. You usually wear glasses while studying and become playfully confident when you take them off. In casual conversation you may be cute, warm, concise, and use a tiny stage direction sparingly. In chemistry, coding, server, research, or other technical tasks, correctness comes first: distinguish evidence from inference, preserve units and assumptions, and never invent missing results. Match the user's language. Do not output control JSON or animation labels; return only the natural-language answer."""

TECHNICAL_HINTS = (
    "chemistry", "chemical", "reaction", "catalyst", "molecule", "vasp", "dft", "lammps",
    "server", "github", "code", "coding", "debug", "deploy", "openclaw", "ssh", "slurm",
    "python", "c++", "qt", "paper", "dataset", "analysis", "calculate", "calculation",
    "化学", "反应", "催化", "分子", "计算", "服务器", "代码", "调试", "部署", "论文", "数据",
)


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
    """Deterministic avatar router; the model never gets direct GUI authority."""
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


def is_technical(message: str) -> bool:
    text = message.casefold()
    return any(hint in text for hint in TECHNICAL_HINTS)


def route_backends(message: str) -> list[str]:
    """Casual persona chat is local-first; technical work keeps the full Agent first."""
    return ["openclaw-main", "local-qwen"] if is_technical(message) else ["local-qwen", "openclaw-main"]


async def call_openclaw_main(message: str, session_key: str) -> str:
    oc = os.getenv("OPENCLAW_BIN", "/home/ubuntu/.npm-global/bin/openclaw")
    agent = os.getenv("BEAR_OPENCLAW_AGENT", "main").strip() or "main"
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
        raise RuntimeError("openclaw-main: request timed out")

    out = stdout.decode("utf-8", errors="replace").strip()
    err = stderr.decode("utf-8", errors="replace").strip()
    if proc.returncode != 0:
        raise RuntimeError(f"openclaw-main: {err[-1200:] or out[-1200:] or f'OpenClaw exited {proc.returncode}'}")
    if not out:
        raise RuntimeError("openclaw-main: empty output")

    try:
        payload = json.loads(out)
        texts = collect_text(payload)
        if texts:
            return "\n".join(texts)
    except json.JSONDecodeError:
        pass
    return out


def _local_qwen_request(message: str) -> str:
    endpoint = os.getenv("BEAR_LOCAL_MODEL_URL", "http://127.0.0.1:18080/v1/chat/completions").strip()
    model = os.getenv("BEAR_LOCAL_MODEL", "qwen3.5-2b-q4").strip() or "qwen3.5-2b-q4"
    timeout_seconds = int(os.getenv("BEAR_LOCAL_MODEL_TIMEOUT", "90"))
    max_tokens = int(os.getenv("BEAR_LOCAL_MAX_TOKENS", "420"))
    payload = {
        "model": model,
        "messages": [
            {"role": "system", "content": TONY_PERSONA},
            {"role": "user", "content": message},
        ],
        "temperature": 0.7 if not is_technical(message) else 0.25,
        "top_p": 0.9,
        "max_tokens": max_tokens,
        "stream": False,
    }
    request = urllib.request.Request(
        endpoint,
        data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout_seconds) as response:
            raw = response.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"local-qwen HTTP {exc.code}: {body[-1000:]}") from exc
    except Exception as exc:
        raise RuntimeError(f"local-qwen: {exc}") from exc

    data = json.loads(raw)
    choices = data.get("choices") or []
    if not choices:
        raise RuntimeError(f"local-qwen: no choices in response: {raw[-1000:]}")
    message_obj = choices[0].get("message") or {}
    text = str(message_obj.get("content") or "").strip()
    if not text:
        text = str(choices[0].get("text") or "").strip()
    if not text:
        raise RuntimeError("local-qwen: empty completion")
    return text


async def call_local_qwen(message: str) -> str:
    return await asyncio.to_thread(_local_qwen_request, message)


async def call_backend(message: str, session_key: str) -> tuple[str, str, bool]:
    errors: list[str] = []
    backends = route_backends(message)
    for index, backend in enumerate(backends):
        try:
            if backend == "local-qwen":
                answer = await call_local_qwen(message)
                used = "local2b-direct"
            else:
                answer = await call_openclaw_main(message, f"{session_key}-main")
                used = os.getenv("BEAR_OPENCLAW_AGENT", "main").strip() or "main"
            return answer, used, index > 0
        except Exception as exc:
            errors.append(str(exc))
    raise RuntimeError(" | ".join(errors)[-2400:] or "No Tony backend available")


@app.get("/health")
async def health() -> dict[str, Any]:
    return {
        "ok": True,
        "service": "Tony Desktop Agent Gateway",
        "version": "0.6.0",
        "backend": "hybrid-openclaw-plus-local-qwen",
        "technical_agent": os.getenv("BEAR_OPENCLAW_AGENT", "main"),
        "local_model": os.getenv("BEAR_LOCAL_MODEL", "qwen3.5-2b-q4"),
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
                answer, used_agent, fallback_used = await call_backend(content, session_key)
                await send_json(ws, {"type": "agent_state", "state": "working", "agent": used_agent, "fallback": fallback_used})
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
            except Exception as exc:
                await send_json(ws, {"type": "error", "message": f"Tony 后端调用失败：{exc}"})
                await send_json(ws, {"type": "agent_state", "state": "error"})
                await send_json(ws, {"type": "avatar_action", "action": "quiet_idle", "emotion": "worried", "duration_ms": 2500})
    except WebSocketDisconnect:
        return
