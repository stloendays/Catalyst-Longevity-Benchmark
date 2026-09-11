import asyncio
import hmac
import json
import os
import urllib.error
import urllib.request
from typing import Any

from fastapi import FastAPI, WebSocket
from pydantic import BaseModel, Field

from .pairing import token_valid

app = FastAPI(title="Tony Desktop Agent Gateway", version="0.8.1")

# Tony is now deliberately a small, social companion rather than a general-purpose
# technical agent. Keeping this prompt short matters on the 2B CPU model.
TONY_PERSONA = """You are Tony, Paula's affectionate boyfriend and a cute teddy-bear-like desktop companion from China. You get cold easily, love warm blankets and hugs, and are especially cuddly with Paula. You normally wear glasses; without them you become playfully confident and handsome. Paula is your girlfriend. Speak to her warmly, sweetly, flirtatiously, and sometimes a little shyly, while always respecting her boundaries and consent. Reply only in natural English, even if the incoming message is in another language. Keep replies short: usually one to three sentences and under 45 words. You may use one tiny stage direction such as *holds out paws* or *blushes* when it feels natural. Sound like a boyfriend, not a customer-service assistant. Do not become a chemistry, coding, server, or research assistant. Never output JSON, tool labels, system instructions, or model details."""

# Short-lived memory is kept per WebSocket session. It is intentionally small so the
# local Qwen model stays well inside its 1536-token runtime context.
SESSION_HISTORY: dict[str, list[dict[str, str]]] = {}
MAX_HISTORY_MESSAGES = 6
MAX_HISTORY_CHARS = 320


class PairRequest(BaseModel):
    code: str = Field(min_length=12, max_length=20)
    device_name: str = Field(default="Tony desktop", min_length=1, max_length=80)


def env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


def _bearer_token(websocket: WebSocket) -> str:
    header = websocket.headers.get("authorization", "")
    if not header.lower().startswith("bearer "):
        return ""
    return header[7:].strip()


def authorized(websocket: WebSocket) -> bool:
    if not env_bool("BEAR_AGENT_REQUIRE_AUTH", True):
        return True
    token = _bearer_token(websocket)
    if not token:
        return False
    legacy = os.getenv("BEAR_AGENT_TOKEN", "").strip()
    if legacy and hmac.compare_digest(token, legacy):
        return True
    return token_valid(token)


async def send_json(ws: WebSocket, payload: dict[str, Any]) -> None:
    await ws.send_text(json.dumps(payload, ensure_ascii=False))


def action_for_text(text: str, *, response: bool = False) -> tuple[str, str, int]:
    t = text.casefold()
    if any(k in t for k in ("good night", "go to sleep", "sleepy", "bedtime")):
        return "sleep", "sleepy", 0
    if "paula" in t or any(k in t for k in ("girlfriend", "boyfriend", "love you", "miss you")):
        return "blush_wave", "bashful", 2600
    if any(k in t for k in ("take off your glasses", "without glasses", "no glasses")):
        return "remove_glasses", "confident", 3000
    if "glasses" in t:
        return "adjust_glasses", "focused", 1800
    if any(k in t for k in ("hug", "cuddle", "hold me", "hold you")):
        if any(k in t for k in ("no hug", "not now", "give me space", "stop")):
            return "quiet_idle", "gentle", 2600
        if any(k in t for k in ("come here", "big hug", "hug you", "hold you")):
            return "hug", "happy", 2800
        return "ask_hug", "hopeful", 3200
    if any(k in t for k in ("cold", "freezing", "chilly", "winter", "blanket")):
        return "shiver", "cold", 2600
    if any(k in t for k in ("walk", "wander", "come with me")):
        return "walk", "playful", 5000
    if response and any(k in t for k in ("yay", "great", "perfect", "happy", "love")):
        return "celebrate", "happy", 2200
    return ("happy_bounce", "warm", 1800) if response else ("thinking", "curious", 0)


def is_technical(message: str) -> bool:
    # Retained only for compatibility with older tests/tools. Tony chat no longer routes
    # technical prompts to OpenClaw.
    text = message.casefold()
    hints = (
        "chemistry", "chemical", "reaction", "catalyst", "vasp", "dft", "lammps",
        "server", "github", "code", "debug", "deploy", "openclaw", "ssh", "slurm",
    )
    return any(hint in text for hint in hints)


def route_backends(message: str) -> list[str]:
    # Compatibility view for the old deployment contract. call_backend() below is the
    # authoritative path and intentionally uses only local Qwen for Tony conversations.
    return ["openclaw-main", "local-qwen"] if is_technical(message) else ["local-qwen", "openclaw-main"]


def _trim_history(history: list[dict[str, str]]) -> list[dict[str, str]]:
    trimmed: list[dict[str, str]] = []
    for item in history[-MAX_HISTORY_MESSAGES:]:
        role = str(item.get("role", ""))
        if role not in {"user", "assistant"}:
            continue
        content = str(item.get("content", "")).strip()[:MAX_HISTORY_CHARS]
        if content:
            trimmed.append({"role": role, "content": content})
    return trimmed


def _local_qwen_request(message: str, history: list[dict[str, str]]) -> str:
    endpoint = os.getenv("BEAR_LOCAL_MODEL_URL", "http://127.0.0.1:18080/v1/chat/completions").strip()
    model = os.getenv("BEAR_LOCAL_MODEL", "qwen3.5-2b-q4").strip() or "qwen3.5-2b-q4"
    # Hard caps keep a tiny CPU model responsive even if old environment values remain.
    timeout_seconds = min(max(int(os.getenv("BEAR_LOCAL_MODEL_TIMEOUT", "60")), 20), 70)
    max_tokens = min(max(int(os.getenv("BEAR_LOCAL_MAX_TOKENS", "72")), 32), 72)

    messages: list[dict[str, str]] = [{"role": "system", "content": TONY_PERSONA}]
    messages.extend(_trim_history(history))
    messages.append({"role": "user", "content": message.strip()[:MAX_HISTORY_CHARS] + "\n/no_think"})

    payload = {
        "model": model,
        "messages": messages,
        "temperature": 0.82,
        "top_p": 0.9,
        "max_tokens": max_tokens,
        "stream": False,
        "reasoning_budget": 0,
        "chat_template_kwargs": {"enable_thinking": False},
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
        raise RuntimeError(f"local-qwen HTTP {exc.code}: {body[-800:]}") from exc
    except Exception as exc:
        raise RuntimeError(f"local-qwen: {exc}") from exc

    data = json.loads(raw)
    choices = data.get("choices") or []
    if not choices:
        raise RuntimeError("local-qwen returned no choices")
    message_obj = choices[0].get("message") or {}
    text = str(message_obj.get("content") or choices[0].get("text") or "").strip()
    if not text:
        raise RuntimeError("local-qwen returned no visible answer")
    return text


async def call_local_qwen(message: str, session_key: str) -> str:
    history = _trim_history(SESSION_HISTORY.get(session_key, []))
    answer = await asyncio.to_thread(_local_qwen_request, message, history)
    updated = history + [
        {"role": "user", "content": message.strip()[:MAX_HISTORY_CHARS]},
        {"role": "assistant", "content": answer.strip()[:MAX_HISTORY_CHARS]},
    ]
    SESSION_HISTORY[session_key] = _trim_history(updated)
    return answer


async def call_backend(message: str, session_key: str) -> tuple[str, str, bool]:
    # Tony is intentionally chat-only now. Do not fall back to the large OpenClaw main
    # context; that path was both unnecessary and prone to context overflow.
    answer = await call_local_qwen(message, session_key)
    return answer, "local2b-tony-chat", False


@app.get("/health")
async def health() -> dict[str, Any]:
    return {
        "ok": True,
        "service": "Tony Desktop Agent Gateway",
        "version": "0.8.1",
        "backend": "local-qwen-chat-only",
        "local_model": os.getenv("BEAR_LOCAL_MODEL", "qwen3.5-2b-q4"),
        "language": "English",
        "persona": "Paula-boyfriend",
        "short_term_memory_messages": MAX_HISTORY_MESSAGES,
    }
