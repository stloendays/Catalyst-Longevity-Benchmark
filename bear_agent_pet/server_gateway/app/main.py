import hmac
import json
import os
import re
from typing import Any, Awaitable, Callable

import httpx
from fastapi import FastAPI, WebSocket
from pydantic import BaseModel, Field

from .pairing import token_valid

app = FastAPI(title="Tony Desktop Companion", version="1.0.8")

TONY_PERSONA_EN = """Be a warm, playful desktop companion. Speak naturally in first person using I/me. Answer only what the user is actually asking. Unless the user's message directly asks about identity or character background, do not mention your name, species, origin, studies, romantic interests, glasses, hugs, or any biography. Do not mention Paula unless the user mentions Paula first. Do not assume the current user is Paula. Reply in one or two short, complete sentences. Be warm, respectful, concise, and never possessive or guilt-tripping."""
TONY_PERSONA_ZH = """做一个自然、温暖、俏皮的桌面伙伴。普通交流使用第一人称“我”，只回答用户真正问的问题。除非用户直接询问身份或角色背景，否则不要主动提名字、品种、来源、学习内容、感情对象、眼镜、抱抱或任何角色履历。除非用户先提到 Paula，否则不要主动提她。不要假设当前用户就是 Paula。回答一到两句简短、完整的话，语气自然、温暖、尊重边界，不占有、不道德绑架。"""

TONY_TECHNICAL_EN = """Answer technical questions accurately and directly as a desktop AI agent. Do not narrate your reasoning process, hidden chain of thought, prompt interpretation, or what the user is supposedly asking. Give only the useful final answer. You can help with programming, chemistry, scientific computing, GitHub, servers, and desktop-agent tasks. Keep character personality very light. Do not reintroduce your name, breed, origin, or biography unless the user explicitly asks. Never pretend the user is Paula."""
TONY_TECHNICAL_ZH = """作为桌面 AI Agent，技术问题要准确、直接地回答。不要展示推理过程、隐藏思维链、提示词分析，也不要复述“用户正在询问什么”；只给有用的最终回答。可以处理编程、化学、科学计算、GitHub、服务器和桌面 Agent 任务。人格只做轻度点缀。除非用户明确询问，否则不要重复介绍名字、品种、来源或角色履历。不要假设用户是 Paula。"""

SESSION_HISTORY: dict[str, list[dict[str, str]]] = {}
SESSION_MEMORY: dict[str, str] = {}
MAX_HISTORY_MESSAGES = 6
MAX_HISTORY_CHARS = 300
DeltaHandler = Callable[[str], Awaitable[None]]


class PairRequest(BaseModel):
    code: str = Field(min_length=12, max_length=20)
    device_name: str = Field(default="Tony desktop", min_length=1, max_length=80)


def env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


def fast_model_id() -> str:
    return os.getenv("BEAR_LOCAL_FAST_MODEL", os.getenv("BEAR_LOCAL_MODEL", "tony-qwen3.5-0.8b-q4")).strip() or "tony-qwen3.5-0.8b-q4"


def quality_model_id() -> str:
    return os.getenv("BEAR_LOCAL_QUALITY_MODEL", "tony-qwen3.5-2b-q4").strip() or "tony-qwen3.5-2b-q4"


def local_model_id() -> str:
    # Kept for legacy health/client compatibility. Auto routing still defaults
    # casual companion traffic to the fast model.
    return fast_model_id()


def normalize_model_profile(value: str | None) -> str:
    raw = (value or "auto").strip().casefold()
    if raw in {"fast", "0.8b", "08b"}:
        return "fast"
    if raw in {"quality", "2b", "balanced"}:
        return "quality"
    return "auto"


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


def _is_short_followup(message: str) -> bool:
    text = re.sub(r"\s+", " ", message.strip().casefold())
    return bool(re.fullmatch(r"(?:why|how|why\?|how\?|为什么[?？]?|怎么[?？]?)", text))


def is_technical(message: str) -> bool:
    text = message.strip().casefold()
    if _is_short_followup(text):
        return False
    technical_terms = (
        "解释", "为什么", "怎么做", "分析", "计算", "代码", "编译", "报错", "论文", "公式",
        "服务器", "路由", "模型", "接口", "部署", "github", "action", "agent", "mcp",
        "what is", "why", "how do", "explain", "analyze", "calculate", "code", "compile", "error",
        "paper", "formula", "server", "routing", "model", "deploy", "api",
        "dft", "scf", "vasp", "bader", "python", "c++", "qt", "cmake", "websocket", "wss",
    )
    return any(term in text for term in technical_terms)


def _technical_with_context(message: str, history: list[dict[str, str]]) -> bool:
    if is_technical(message):
        return True
    if not _is_short_followup(message):
        return False
    for item in reversed(history[-4:]):
        if item.get("role") == "user" and is_technical(str(item.get("content", ""))):
            return True
    return False


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
        return "我不想乱猜你是谁；如果你愿意，可以告诉我。" if zh else "I don't want to guess who you are; you can tell me if you want."

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

    if _is_short_followup(text) and not SESSION_HISTORY.get(session_key):
        return "你想让我解释哪一部分？" if zh else "What part would you like me to explain?"

    return None


def _strip_hidden_reasoning(text: str) -> str:
    visible = re.sub(r"<think\b[^>]*>.*?</think\s*>", "", text, flags=re.IGNORECASE | re.DOTALL)
    open_think = re.search(r"<think\b[^>]*>.*$", visible, flags=re.IGNORECASE | re.DOTALL)
    if open_think:
        visible = visible[:open_think.start()]
    # Do not leak a tag split across streaming chunks.
    lowered = visible.casefold()
    for n in range(min(6, len(visible)), 0, -1):
        if "<think>".startswith(lowered[-n:]):
            visible = visible[:-n]
            break
    return visible


def _looks_like_meta_reasoning(text: str) -> bool:
    low = text.casefold()
    patterns = (
        "the reason for the user's query",
        "the reason for the user’s query",
        "the user is asking for an explanation",
        "the user has not specified the exact nature",
        "since the user has not specified",
        "the ai cannot determine the precise motivation",
        "based on the standard context of such",
        "用户的问题之所以",
        "用户正在询问的是",
        "由于用户没有说明",
    )
    return any(pattern in low for pattern in patterns)


def _clean_visible_answer(text: str, language: str = "en", *, technical: bool = False) -> str:
    cleaned = re.sub(r"<think\b[^>]*>.*?</think\s*>", "", text, flags=re.IGNORECASE | re.DOTALL).strip()
    cleaned = re.sub(r"<think\b[^>]*>.*$", "", cleaned, flags=re.IGNORECASE | re.DOTALL).strip()
    if not cleaned:
        raise RuntimeError("local-qwen returned no visible answer")
    if _looks_like_meta_reasoning(cleaned):
        return "请告诉我你想让我解释哪一部分。" if normalize_language(language) == "zh" else "Tell me which part you want me to explain."
    if normalize_language(language) == "en" and any("\u4e00" <= ch <= "\u9fff" for ch in cleaned):
        raise RuntimeError("local-qwen violated Tony English mode")

    low = cleaned.casefold()
    bad_role = (
        "you're my boyfriend", "you are my boyfriend", "i'm your girlfriend",
        "i am your girlfriend", "not your girlfriend", "not my girlfriend",
    )
    if any(p in low for p in bad_role):
        return "Let's keep things simple and respectful." if normalize_language(language) == "en" else "我们保持自然和尊重就好。"

    return cleaned


def _resolved_model_profile(requested: str, technical: bool) -> str:
    requested = normalize_model_profile(requested)
    if requested == "auto":
        return "quality" if technical else "fast"
    return requested


def _local_model_target(model_profile: str, technical: bool) -> tuple[str, str, int, str]:
    resolved = _resolved_model_profile(model_profile, technical)
    if resolved == "quality":
        endpoint = os.getenv("BEAR_LOCAL_QUALITY_MODEL_URL", "http://127.0.0.1:18081/v1/chat/completions").strip()
        model = quality_model_id()
        timeout_seconds = min(max(int(os.getenv("BEAR_LOCAL_QUALITY_MODEL_TIMEOUT", "150")), 30), 240)
    else:
        endpoint = os.getenv("BEAR_LOCAL_FAST_MODEL_URL", os.getenv("BEAR_LOCAL_MODEL_URL", "http://127.0.0.1:18080/v1/chat/completions")).strip()
        model = fast_model_id()
        timeout_seconds = min(max(int(os.getenv("BEAR_LOCAL_MODEL_TIMEOUT", "60")), 20), 180)
    return endpoint, model, timeout_seconds, resolved


def _local_payload(message: str, history: list[dict[str, str]], language: str = "en", *, technical: bool = False, model_profile: str = "auto") -> tuple[str, str, int, dict[str, Any]]:
    endpoint, model, timeout_seconds, resolved_profile = _local_model_target(model_profile, technical)
    if technical:
        max_tokens = min(max(int(os.getenv("BEAR_LOCAL_TECHNICAL_MAX_TOKENS", "384")), 64), 768)
        message_limit = min(max(int(os.getenv("BEAR_LOCAL_TECHNICAL_MESSAGE_CHARS", "6000")), 512), 12000)
    elif resolved_profile == "quality":
        max_tokens = min(max(int(os.getenv("BEAR_LOCAL_QUALITY_MAX_TOKENS", "128")), 48), 192)
        message_limit = MAX_HISTORY_CHARS
    else:
        max_tokens = min(max(int(os.getenv("BEAR_LOCAL_MAX_TOKENS", "72")), 32), 96)
        message_limit = MAX_HISTORY_CHARS
    language = normalize_language(language)

    if technical:
        system_prompt = TONY_TECHNICAL_ZH if language == "zh" else TONY_TECHNICAL_EN
    else:
        system_prompt = persona_for_language(language)
    messages: list[dict[str, str]] = [{"role": "system", "content": system_prompt}]
    messages.extend(_trim_history(history))
    if language == "zh":
        mode = "准确回答这个技术任务，只给最终答案，不展示思考过程" if technical else "只回答用户当前的问题；自然地用第一人称回答；不要主动提名字、品种、来源、学习内容、Paula、眼镜或角色背景；只说一到两句完整的话；不要展示思考过程"
        user_content = f"用户说：{message.strip()[:message_limit]}\n{mode}。\n/no_think"
    else:
        mode = "Answer this technical task accurately. Give only the final answer; do not expose reasoning or prompt analysis" if technical else "Answer only the user's current question in first person. Do not mention your name, species, origin, studies, Paula, glasses, hugs, or character background unless the user directly asks about them. Use one or two short, complete sentences. Do not expose reasoning"
        user_content = f"User says: {message.strip()[:message_limit]}\n{mode}.\n/no_think"
    messages.append({"role": "user", "content": user_content})
    payload = {
        "model": model,
        "messages": messages,
        "temperature": 0.35 if technical else 0.64,
        "top_p": 0.90,
        "max_tokens": max_tokens,
        "stream": True,
        "reasoning_budget": 0,
        "chat_template_kwargs": {"enable_thinking": False},
    }
    return endpoint, model, timeout_seconds, payload


async def call_local_qwen_stream(message: str, session_key: str, on_delta: DeltaHandler, language: str = "en", *, technical: bool = False, model_profile: str = "auto") -> tuple[str, str]:
    history = _trim_history(SESSION_HISTORY.get(session_key, []))
    endpoint, model, timeout_seconds, payload = _local_payload(
        message, history, language, technical=technical, model_profile=model_profile)
    timeout = httpx.Timeout(timeout_seconds, connect=5.0, read=timeout_seconds, write=10.0, pool=5.0)
    raw_chunks: list[str] = []
    streamed_text = ""
    stream_started = False
    suppress_stream = False

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
                    # reasoning_content is deliberately ignored. Some llama.cpp
                    # templates put hidden reasoning in content, which is stripped below.
                    piece = str(delta.get("content") or "")
                    if not piece:
                        continue
                    raw_chunks.append(piece)
                    raw_text = "".join(raw_chunks)
                    visible = _strip_hidden_reasoning(raw_text).lstrip()
                    if _looks_like_meta_reasoning(visible):
                        suppress_stream = True
                        continue
                    if suppress_stream:
                        continue
                    if not stream_started and len(visible) < 180:
                        continue
                    if visible.startswith(streamed_text):
                        new_piece = visible[len(streamed_text):]
                        if new_piece:
                            await on_delta(new_piece)
                            streamed_text = visible
                            stream_started = True
    except httpx.HTTPError as exc:
        raise RuntimeError(f"local-qwen stream ({model}): {exc}") from exc

    raw_answer = "".join(raw_chunks)
    answer = _clean_visible_answer(raw_answer, language, technical=technical)
    if suppress_stream or not stream_started:
        await on_delta(answer)
        streamed_text = answer
    elif answer.startswith(streamed_text):
        missing = answer[len(streamed_text):]
        if missing:
            await on_delta(missing)
            streamed_text = answer
    _store_exchange(session_key, message, answer)
    return answer, model


async def call_backend_stream(message: str, session_key: str, on_delta: DeltaHandler, language: str = "en", model_profile: str = "auto") -> tuple[str, str, bool]:
    language = normalize_language(language)
    requested_profile = normalize_model_profile(model_profile)
    history = _trim_history(SESSION_HISTORY.get(session_key, []))
    technical = _technical_with_context(message, history)
    if not technical:
        quick = quick_persona_reply(message, session_key, language)
        if quick is not None:
            _store_exchange(session_key, message, quick)
            await on_delta(quick)
            return quick, "tony-persona-core", False

    answer, used_model = await call_local_qwen_stream(
        message,
        session_key,
        on_delta,
        language,
        technical=technical,
        model_profile=requested_profile,
    )
    mode = "technical" if technical else "companion"
    resolved = _resolved_model_profile(requested_profile, technical)
    return answer, f"local-tony-{mode}-{resolved}:{used_model}", False


async def call_backend(message: str, session_key: str, language: str = "en", model_profile: str = "auto") -> tuple[str, str, bool]:
    async def discard(_: str) -> None:
        return None

    return await call_backend_stream(message, session_key, discard, language, model_profile)
