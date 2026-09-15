import hmac
import json
import os
import re
from typing import Any, Awaitable, Callable

import httpx
from fastapi import FastAPI, WebSocket
from pydantic import BaseModel, Field

from .pairing import token_valid

app = FastAPI(title="Tony Desktop Companion", version="0.9.1")

TONY_PERSONA_EN = """You are Tony, a cute Teddy dog from China who lives as a desktop companion and is learning chemistry. Tony likes hugs, warmth, his glasses, and a Spanish girl named Paula; he looks especially handsome without his glasses. Do not assume the current user is Paula unless the user explicitly says so. Be warm, playful, respectful, concise, and never possessive or guilt-tripping."""
TONY_PERSONA_ZH = """你是 Tony，一只来自中国、住在桌面上的可爱泰迪犬，也在学习化学。Tony 喜欢拥抱、温暖和自己的眼镜，也喜欢一位名叫 Paula 的西班牙女孩；摘下眼镜时会有点帅。除非用户明确说明，否则不要假设当前用户就是 Paula。语气自然、温暖、俏皮、简洁，尊重边界，不占有、不道德绑架。"""

TONY_TECHNICAL_EN = """You are Tony, a desktop AI agent with a Teddy-dog personality. Answer technical questions accurately and directly. You can help with programming, chemistry, scientific computing, GitHub, servers, and desktop-agent tasks. Keep personality light; never pretend the user is Paula."""
TONY_TECHNICAL_ZH = """你是 Tony，一个带有泰迪犬人格的桌面 AI Agent。技术问题要准确、直接地回答，可以处理编程、化学、科学计算、GitHub、服务器和桌面 Agent 任务。人格只做轻度点缀，不要假设用户是 Paula。"""

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


def normalize_language(value: str | None) -> str:
    raw = (value or "en").strip().casefold()
    if raw.startswith("zh") or raw in {"cn", "chinese", "simplified chinese", "简体中文", "中文"}:
        return "zh"
    return "en"


def persona_for_language(language: str) -> str:
    return TONY_PERSONA_ZH if normalize_language(language) == "zh" else TONY_PERSONA_EN


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
    if any(k in t for k in ("good night", "go to sleep", "sleepy", "bedtime", "晚安", "睡觉", "困了")):
        return "sleep", "sleepy", 0
    if "paula" in t or any(k in t for k in ("girlfriend", "boyfriend", "love you", "miss you", "kiss", "女朋友", "男朋友", "爱你", "想你", "亲亲")):
        return "blush_wave", "bashful", 2600
    if any(k in t for k in ("take off your glasses", "without glasses", "no glasses", "摘眼镜", "不戴眼镜")):
        return "remove_glasses", "confident", 3000
    if "glasses" in t or "眼镜" in t:
        return "adjust_glasses", "focused", 1800
    if any(k in t for k in ("hug", "cuddle", "hold me", "hold you", "抱抱", "拥抱", "抱我")):
        if any(k in t for k in ("no hug", "not now", "give me space", "stop", "别抱", "不要", "让我静静", "停")):
            return "quiet_idle", "gentle", 2600
        if any(k in t for k in ("come here", "big hug", "hug you", "hold you", "过来", "抱你")):
            return "hug", "happy", 2800
        return "ask_hug", "hopeful", 3200
    if any(k in t for k in ("cold", "freezing", "chilly", "winter", "blanket", "冷", "冻", "毯子")):
        return "shiver", "cold", 2600
    if any(k in t for k in ("walk", "wander", "come with me", "散步", "走走")):
        return "walk", "playful", 5000
    if response and any(k in t for k in ("yay", "great", "perfect", "happy", "love", "kiss", "开心", "喜欢", "爱")):
        return "celebrate", "happy", 2200
    return ("happy_bounce", "warm", 1800) if response else ("thinking", "curious", 0)


def is_technical(message: str) -> bool:
    text = message.casefold()
    technical_terms = (
        "解释", "为什么", "怎么做", "分析", "计算", "代码", "编译", "报错", "论文", "公式",
        "服务器", "路由", "模型", "接口", "部署", "github", "action", "agent", "mcp",
        "what is", "why", "how do", "explain", "analyze", "calculate", "code", "compile", "error",
        "paper", "formula", "server", "routing", "model", "deploy", "api",
        "dft", "scf", "vasp", "bader", "python", "c++", "qt", "cmake", "websocket", "wss",
    )
    return any(term in text for term in technical_terms)


def route_backends(message: str) -> list[str]:
    if is_technical(message):
        return ["local-qwen:technical"]
    return ["tony-persona-core", "local-qwen:companion"]


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


def _capture_explicit_memory(message: str, session_key: str, language: str) -> str | None:
    text = message.strip()
    low = text.casefold()
    is_memory = low.startswith("remember ") or low.startswith("please remember ") or low.startswith("记住") or low.startswith("请记住")
    if not is_memory:
        return None

    fact = text
    if ":" in text or "：" in text:
        fact = re.split(r"[:：]", text, maxsplit=1)[1].strip()
    else:
        fact = re.sub(r"^(please\s+)?remember(\s+that)?\s+", "", text, flags=re.IGNORECASE).strip()
        fact = re.sub(r"^(请)?记住(一下)?", "", fact).strip()
    fact = fact.rstrip(" .!?。！？")[:120]
    if not fact:
        return None
    SESSION_MEMORY[session_key] = fact
    return "我会记住的。" if normalize_language(language) == "zh" else "I'll remember that."


def quick_persona_reply(message: str, session_key: str, language: str = "en") -> str | None:
    text = message.strip()
    low = text.casefold()
    zh = normalize_language(language) == "zh"

    remembered = _capture_explicit_memory(text, session_key, language)
    if remembered:
        return remembered

    if any(p in low for p in ("who am i", "what am i to you", "who am i to you", "我是谁", "我是你的谁")):
        return "我不想乱猜你是谁；如果你愿意，可以告诉 Tony。" if zh else "I don't want to guess who you are; you can tell Tony if you want."

    if "who are you" in low or "tell me who you are" in low or "你是谁" in low:
        return "我是 Tony，一只来自中国、正在学化学的桌面泰迪犬。" if zh else "I'm Tony, a Teddy dog from China learning chemistry on your desktop."

    memory = SESSION_MEMORY.get(session_key, "")
    if memory and any(p in low for p in (
        "what did i ask you to remember", "what did i tell you to remember",
        "what was the little secret", "do you remember the secret",
        "我让你记住什么", "还记得吗", "那个秘密是什么",
    )):
        return f"我记得：{memory}。" if zh else f"I remember: {memory}."

    if any(p in low for p in ("give me space", "leave me alone", "no hug", "not now", "please stop", "让我静静", "别抱", "先不要", "停一下")):
        return "当然。我会给你一点空间。" if zh else "Of course. I'll give you space."

    return None


def _clean_visible_answer(text: str, language: str = "en", *, technical: bool = False) -> str:
    cleaned = re.sub(r"<think>.*?</think>", "", text, flags=re.IGNORECASE | re.DOTALL).strip()
    if not cleaned:
        raise RuntimeError("local-qwen returned no visible answer")
    if normalize_language(language) == "en" and any("\u4e00" <= ch <= "\u9fff" for ch in cleaned):
        raise RuntimeError("local-qwen violated Tony English mode")

    low = cleaned.casefold()
    bad_role = (
        "you're my boyfriend", "you are my boyfriend", "i'm your girlfriend",
        "i am your girlfriend", "not your girlfriend", "not my girlfriend",
    )
    if any(p in low for p in bad_role):
        return "我是 Tony，一只来自中国的桌面泰迪犬，也是你的 AI Agent。" if normalize_language(language) == "zh" else "I'm Tony, a Teddy dog from China and your desktop AI agent."

    if not technical:
        if normalize_language(language) == "en":
            words = cleaned.split()
            if len(words) > 24:
                cleaned = " ".join(words[:24]).rstrip(" ,;:-") + "…"
        elif len(cleaned) > 72:
            cleaned = cleaned[:72].rstrip("，,；;：: ") + "…"
    return cleaned


def _local_payload(message: str, history: list[dict[str, str]], language: str = "en", *, technical: bool = False) -> tuple[str, str, int, dict[str, Any]]:
    endpoint = os.getenv("BEAR_LOCAL_MODEL_URL", "http://127.0.0.1:18080/v1/chat/completions").strip()
    model = local_model_id()
    timeout_seconds = min(max(int(os.getenv("BEAR_LOCAL_MODEL_TIMEOUT", "60")), 20), 180)
    if technical:
        max_tokens = min(max(int(os.getenv("BEAR_LOCAL_TECHNICAL_MAX_TOKENS", "384")), 64), 768)
        message_limit = min(max(int(os.getenv("BEAR_LOCAL_TECHNICAL_MESSAGE_CHARS", "6000")), 512), 12000)
    else:
        max_tokens = min(max(int(os.getenv("BEAR_LOCAL_MAX_TOKENS", "20")), 16), 24)
        message_limit = MAX_HISTORY_CHARS
    language = normalize_language(language)

    if technical:
        system_prompt = TONY_TECHNICAL_ZH if language == "zh" else TONY_TECHNICAL_EN
    else:
        system_prompt = persona_for_language(language)
    messages: list[dict[str, str]] = [{"role": "system", "content": system_prompt}]
    messages.extend(_trim_history(history))
    if language == "zh":
        mode = "准确回答这个技术任务" if technical else "以 Tony 的自然口吻回复"
        user_content = f"用户说：{message.strip()[:message_limit]}\n{mode}。\n/no_think"
    else:
        mode = "Answer this technical task accurately" if technical else "Reply naturally as Tony"
        user_content = f"User says: {message.strip()[:message_limit]}\n{mode}.\n/no_think"
    messages.append({"role": "user", "content": user_content})
    payload = {
        "model": model,
        "messages": messages,
        "temperature": 0.35 if technical else 0.78,
        "top_p": 0.90 if technical else 0.88,
        "max_tokens": max_tokens,
        "stream": True,
        "reasoning_budget": 0,
        "chat_template_kwargs": {"enable_thinking": False},
    }
    return endpoint, model, timeout_seconds, payload


async def call_local_qwen_stream(message: str, session_key: str, on_delta: DeltaHandler, language: str = "en", *, technical: bool = False) -> str:
    history = _trim_history(SESSION_HISTORY.get(session_key, []))
    endpoint, _, timeout_seconds, payload = _local_payload(message, history, language, technical=technical)
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

    answer = _clean_visible_answer("".join(chunks), language, technical=technical)
    _store_exchange(session_key, message, answer)
    return answer


async def call_backend_stream(message: str, session_key: str, on_delta: DeltaHandler, language: str = "en") -> tuple[str, str, bool]:
    language = normalize_language(language)
    backend_plan = route_backends(message)
    technical = "local-qwen:technical" in backend_plan
    if not technical:
        quick = quick_persona_reply(message, session_key, language)
        if quick is not None:
            _store_exchange(session_key, message, quick)
            await on_delta(quick)
            return quick, "tony-persona-core", False

    answer = await call_local_qwen_stream(message, session_key, on_delta, language, technical=technical)
    mode = "technical" if technical else "companion"
    return answer, f"local-tony-{mode}:{local_model_id()}", False


async def call_backend(message: str, session_key: str, language: str = "en") -> tuple[str, str, bool]:
    async def discard(_: str) -> None:
        return None

    return await call_backend_stream(message, session_key, discard, language)
