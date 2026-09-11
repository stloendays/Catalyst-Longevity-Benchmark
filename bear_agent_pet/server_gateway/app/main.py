import asyncio
import hmac
import json
import os
import re
import urllib.error
import urllib.request
from typing import Any

from fastapi import FastAPI, WebSocket
from pydantic import BaseModel, Field

from .pairing import token_valid

app = FastAPI(title="Tony Desktop Companion", version="0.8.3")

# Tony is a tiny companion model, not a general assistant. Keep the permanent prompt
# small so CPU time is spent on Paula's conversation instead of prompt evaluation.
TONY_PERSONA = """You are Tony, Paula's cute Chinese teddy-bear boyfriend. Paula is your Spanish girlfriend. Reply only in natural English, one warm playful sentence under 14 words. You love hugs, blankets, warmth and your glasses; handsome without them. Respect no or requests for space. Never be possessive or guilt-trip. No technical help."""

SESSION_HISTORY: dict[str, list[dict[str, str]]] = {}
MAX_HISTORY_MESSAGES = 4
MAX_HISTORY_CHARS = 120


class PairRequest(BaseModel):
    code: str = Field(min_length=12, max_length=20)
    device_name: str = Field(default="Tony desktop", min_length=1, max_length=80)


def env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


def local_model_id() -> str:
    return os.getenv("BEAR_LOCAL_MODEL", "tony-qwen3.5-0.8b-q4").strip() or "tony-qwen3.5-0.8b-q4"


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
    if "paula" in t or any(k in t for k in ("girlfriend", "boyfriend", "love you", "miss you", "kiss")):
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
    if response and any(k in t for k in ("yay", "great", "perfect", "happy", "love", "kiss")):
        return "celebrate", "happy", 2200
    return ("happy_bounce", "warm", 1800) if response else ("thinking", "curious", 0)


def is_technical(message: str) -> bool:
    return False


def route_backends(message: str) -> list[str]:
    return ["local-qwen"]


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


def _clean_visible_answer(text: str) -> str:
    cleaned = re.sub(r"<think>.*?</think>", "", text, flags=re.IGNORECASE | re.DOTALL).strip()
    cleaned = re.sub(r"^\s*(Tony|Assistant)\s*:\s*", "", cleaned, flags=re.IGNORECASE).strip()
    if not cleaned:
        raise RuntimeError("local-qwen returned no visible answer")
    if any("\u4e00" <= ch <= "\u9fff" for ch in cleaned):
        raise RuntimeError("local-qwen violated Tony English-only mode")
    words = cleaned.split()
    if len(words) > 24:
        cleaned = " ".join(words[:24]).rstrip(" ,;:-") + "…"
    return cleaned


def _local_qwen_request(message: str, history: list[dict[str, str]]) -> str:
    endpoint = os.getenv("BEAR_LOCAL_MODEL_URL", "http://127.0.0.1:18080/v1/chat/completions").strip()
    model = local_model_id()
    timeout_seconds = min(max(int(os.getenv("BEAR_LOCAL_MODEL_TIMEOUT", "45")), 20), 180)
    # The companion only needs one sentence. A hard token ceiling matters more than a
    # large completion budget on this CPU-only host.
    max_tokens = min(max(int(os.getenv("BEAR_LOCAL_MAX_TOKENS", "20")), 16), 20)

    messages: list[dict[str, str]] = [{"role": "system", "content": TONY_PERSONA}]
    messages.extend(_trim_history(history))
    messages.append({"role": "user", "content": message.strip()[:MAX_HISTORY_CHARS] + "\n/no_think"})

    payload = {
        "model": model,
        "messages": messages,
        "temperature": 0.78,
        "top_p": 0.88,
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
    return _clean_visible_answer(text)


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
    answer = await call_local_qwen(message, session_key)
    return answer, f"local-tony-chat:{local_model_id()}", False
