import hmac
import json
import os
import re
from typing import Any, Awaitable, Callable

import httpx
from fastapi import FastAPI, WebSocket
from pydantic import BaseModel, Field

from .pairing import token_valid

app = FastAPI(title="Tony Desktop Companion", version="0.8.4")

# Tony is a tiny companion model, not a general assistant. Stable identity facts and
# explicit memory are handled by a deterministic persona core; the local model is used
# for the creative boyfriend-chat part.
TONY_PERSONA = """You are Tony, Paula's cute Chinese teddy-bear boyfriend. The user is Paula, your Spanish girlfriend. Tony is the boyfriend; Paula is the girlfriend. Never swap those roles. Reply only in natural English, one warm playful sentence under 14 words. You love hugs, blankets, warmth and your glasses; handsome without them. Respect no or requests for space. Never be possessive or guilt-trip. No technical help."""

SESSION_HISTORY: dict[str, list[dict[str, str]]] = {}
SESSION_MEMORY: dict[str, str] = {}
MAX_HISTORY_MESSAGES = 2
MAX_HISTORY_CHARS = 120
DeltaHandler = Callable[[str], Awaitable[None]]


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


def _store_exchange(session_key: str, user_text: str, assistant_text: str) -> None:
    history = _trim_history(SESSION_HISTORY.get(session_key, []))
    history.extend([
        {"role": "user", "content": user_text.strip()[:MAX_HISTORY_CHARS]},
        {"role": "assistant", "content": assistant_text.strip()[:MAX_HISTORY_CHARS]},
    ])
    SESSION_HISTORY[session_key] = _trim_history(history)


def _capture_explicit_memory(message: str, session_key: str) -> str | None:
    text = message.strip()
    low = text.casefold()
    if not (low.startswith("remember ") or low.startswith("please remember ")):
        return None

    fact = text
    if ":" in text:
        fact = text.split(":", 1)[1].strip()
    else:
        fact = re.sub(r"^(please\s+)?remember(\s+that)?\s+", "", text, flags=re.IGNORECASE).strip()
    fact = fact.rstrip(" .!?")[:120]
    if not fact:
        return None
    SESSION_MEMORY[session_key] = fact
    return "I'll remember that, Paula."


def quick_persona_reply(message: str, session_key: str) -> str | None:
    text = message.strip()
    low = text.casefold()

    remembered = _capture_explicit_memory(text, session_key)
    if remembered:
        return remembered

    if any(p in low for p in ("who am i", "what am i to you", "who am i to you")):
        return "You're Paula, my Spanish girlfriend, and I'm your Tony."

    if "who are you" in low or "tell me who you are" in low:
        return "I'm Tony, your cuddly teddy-bear boyfriend from China."

    memory = SESSION_MEMORY.get(session_key, "")
    if memory and any(p in low for p in (
        "what did i ask you to remember",
        "what did i tell you to remember",
        "what was the little secret",
        "do you remember the secret",
    )):
        return f"I remember, Paula: {memory}."

    if any(p in low for p in ("give me space", "leave me alone", "no hug", "not now", "please stop")):
        return "Of course, Paula. I'll give you space."

    return None


def _clean_visible_answer(text: str) -> str:
    cleaned = re.sub(r"<think>.*?</think>", "", text, flags=re.IGNORECASE | re.DOTALL).strip()
    if not cleaned:
        raise RuntimeError("local-qwen returned no visible answer")
    if any("\u4e00" <= ch <= "\u9fff" for ch in cleaned):
        raise RuntimeError("local-qwen violated Tony English-only mode")

    # Guard a few common role inversions from tiny models. Stable relationship facts
    # belong to the persona core, not to model improvisation.
    low = cleaned.casefold()
    bad_role = (
        "you're my boyfriend",
        "you are my boyfriend",
        "i'm your girlfriend",
        "i am your girlfriend",
        "not your girlfriend",
        "not my girlfriend",
    )
    if any(p in low for p in bad_role):
        return "I'm your Tony, Paula—your cuddly boyfriend is right here."

    words = cleaned.split()
    if len(words) > 24:
        cleaned = " ".join(words[:24]).rstrip(" ,;:-") + "…"
    return cleaned


def _local_payload(message: str, history: list[dict[str, str]]) -> tuple[str, str, int, dict[str, Any]]:
    endpoint = os.getenv("BEAR_LOCAL_MODEL_URL", "http://127.0.0.1:18080/v1/chat/completions").strip()
    model = local_model_id()
    timeout_seconds = min(max(int(os.getenv("BEAR_LOCAL_MODEL_TIMEOUT", "60")), 20), 180)
    max_tokens = min(max(int(os.getenv("BEAR_LOCAL_MAX_TOKENS", "20")), 16), 20)

    messages: list[dict[str, str]] = [{"role": "system", "content": TONY_PERSONA}]
    memory = SESSION_MEMORY.get("", "")
    messages.extend(_trim_history(history))
    messages.append({
        "role": "user",
        "content": f"Paula says: {message.strip()[:MAX_HISTORY_CHARS]}\nReply as her boyfriend Tony.\n/no_think",
    })
    payload = {
        "model": model,
        "messages": messages,
        "temperature": 0.78,
        "top_p": 0.88,
        "max_tokens": max_tokens,
        "stream": True,
        "reasoning_budget": 0,
        "chat_template_kwargs": {"enable_thinking": False},
    }
    return endpoint, model, timeout_seconds, payload


async def call_local_qwen_stream(message: str, session_key: str, on_delta: DeltaHandler) -> str:
    history = _trim_history(SESSION_HISTORY.get(session_key, []))
    endpoint, _, timeout_seconds, payload = _local_payload(message, history)
    timeout = httpx.Timeout(timeout_seconds, connect=5.0, read=timeout_seconds, write=10.0, pool=5.0)
    chunks: list[str] = []

    try:
        async with httpx.AsyncClient(timeout=timeout) as client:
            async with client.stream("POST", endpoint, json=payload) as response:
                response.raise_for_status()
                async for line in response.aiter_lines():
                    if not line.startswith("data:"):
                        continue
                    data = line[5:].strip()
                    if not data or data == "[DONE]":
                        if data == "[DONE]":
                            break
                        continue
                    try:
                        event = json.loads(data)
                    except json.JSONDecodeError:
                        continue
                    choices = event.get("choices") or []
                    if not choices:
                        continue
                    delta = choices[0].get("delta") or {}
                    piece = str(delta.get("content") or "")
                    if not piece:
                        continue
                    chunks.append(piece)
                    await on_delta(piece)
    except httpx.HTTPError as exc:
        raise RuntimeError(f"local-qwen stream: {exc}") from exc

    answer = _clean_visible_answer("".join(chunks))
    _store_exchange(session_key, message, answer)
    return answer


async def call_backend_stream(message: str, session_key: str, on_delta: DeltaHandler) -> tuple[str, str, bool]:
    quick = quick_persona_reply(message, session_key)
    if quick is not None:
        _store_exchange(session_key, message, quick)
        await on_delta(quick)
        return quick, "tony-persona-core", False

    answer = await call_local_qwen_stream(message, session_key, on_delta)
    return answer, f"local-tony-chat:{local_model_id()}", False


async def call_backend(message: str, session_key: str) -> tuple[str, str, bool]:
    async def discard(_: str) -> None:
        return None

    return await call_backend_stream(message, session_key, discard)
