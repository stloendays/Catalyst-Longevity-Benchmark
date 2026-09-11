from __future__ import annotations

import json
from pathlib import Path


ROOT = Path("teddy_agent_pet")

# Specific identity statements go first so generic wording substitutions never
# leave Tony ambiguously bear-like. Tony is the Chinese "Teddy" dog concept:
# a toy-poodle-style dog, not a bear.
SPECIFIC = {
    "You are Tony, Paula's cute Chinese teddy-bear boyfriend.":
        "You are Tony, Paula's cute Chinese Teddy dog (toy poodle) boyfriend. You are a dog, never a bear.",
    "你是 Tony，Paula 可爱的中国泰迪熊男朋友。":
        "你是 Tony，Paula 可爱的中国泰迪犬男朋友（toy poodle）。你是狗，不是熊。",
    "I'm Tony, your cuddly teddy-bear boyfriend from China.":
        "I'm Tony, your cuddly Teddy dog boyfriend from China—a toy poodle, not a bear.",
    "我是 Tony，你来自中国的泰迪熊男朋友。":
        "我是 Tony，你来自中国的泰迪犬男朋友，是 toy poodle，不是熊。",
    "- Form: a cute teddy-bear-like boy; soft, round, expressive, cuddly, and highly huggable":
        "- Form: a cute Teddy dog / toy poodle; fluffy, expressive, cuddly, and highly huggable; Tony is a dog, never a bear",
    "You are Tony, a cute teddy-bear-like desktop companion from China.":
        "You are Tony, a cute Teddy dog (toy poodle) desktop companion from China. You are a dog, not a bear.",
    "You are her cute teddy-bear-like boyfriend from China:":
        "You are her cute Teddy dog (toy poodle) boyfriend from China. You are a dog, not a bear:",
    "You are her cute teddy-bear-like boyfriend from China: warm, playful, cuddly, a little shy, and very affectionate.":
        "You are her cute Teddy dog (toy poodle) boyfriend from China: warm, playful, cuddly, a little shy, and very affectionate. You are a dog, never a bear.",
    "Not exactly—but I have very strong teddy-bear energy. Soft, round, cuddly, and happiest when I'm close to someone I trust.":
        "No—I'm a Teddy dog, a toy poodle. I'm fluffy, cuddly, and happiest when I'm close to someone I trust.",
    "Not exactly—but I have very strong Teddy dog energy. Soft, round, cuddly, and happiest when I'm close to someone I trust.":
        "No—I'm a Teddy dog, a toy poodle. I'm fluffy, cuddly, and happiest when I'm close to someone I trust.",
}

GENERIC = [
    ("teddy-bear-like", "Teddy-dog"),
    ("Teddy-bear-like", "Teddy-dog"),
    ("teddy-bear", "Teddy dog"),
    ("Teddy-bear", "Teddy dog"),
    ("teddy bear", "Teddy dog"),
    ("Teddy bear", "Teddy dog"),
    ("teddy-boyfriend", "Teddy-dog boyfriend"),
    ("teddy-hearted", "poodle-hearted"),
    ("teddy-shaped", "poodle-shaped"),
    ("teddy ears", "poodle ears"),
    ("teddy heart", "poodle heart"),
    ("teddy metabolism", "poodle metabolism"),
    ("teddy thermometer", "poodle thermometer"),
    ("teddy settings", "Teddy-dog settings"),
    ("teddy energy", "Teddy-dog energy"),
    ("teddy mode", "Teddy-dog mode"),
    ("teddy chemist", "poodle chemist"),
    ("teddy Tony", "Teddy-dog Tony"),
    ("teddy-like", "Teddy-dog-like"),
    ("tiny-bear", "tiny-poodle"),
    ("tiny bear", "tiny poodle"),
    ("small bears", "small poodles"),
    ("small bear", "small poodle"),
    ("little bear", "little poodle"),
    ("bear ears", "poodle ears"),
    ("bear energy", "poodle energy"),
    ('"teddy"', '"teddy_dog"'),
]

TEXT_SUFFIXES = {".md", ".json", ".jsonl", ".py", ".txt"}

for base in (ROOT / "training",):
    for path in base.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        text = path.read_text(encoding="utf-8")
        original = text
        for old, new in SPECIFIC.items():
            text = text.replace(old, new)
        for old, new in GENERIC:
            text = text.replace(old, new)
        if text != original:
            path.write_text(text, encoding="utf-8")

# Runtime persona is patched explicitly, including new TEDDY_* environment names
# with BEAR_* fallback aliases so existing server installations keep working.
main = ROOT / "server_gateway" / "app" / "main.py"
text = main.read_text(encoding="utf-8")
for old, new in SPECIFIC.items():
    text = text.replace(old, new)

text = text.replace(
    'def local_model_id() -> str:\n    return os.getenv("BEAR_LOCAL_MODEL", "tony-qwen3.5-0.8b-q4").strip() or "tony-qwen3.5-0.8b-q4"',
    'def env_value(primary: str, legacy: str, default: str) -> str:\n    value = os.getenv(primary)\n    if value is None:\n        value = os.getenv(legacy)\n    return (value if value is not None else default).strip()\n\n\ndef local_model_id() -> str:\n    return env_value("TEDDY_LOCAL_MODEL", "BEAR_LOCAL_MODEL", "tony-qwen3.5-0.8b-q4") or "tony-qwen3.5-0.8b-q4"',
)
text = text.replace(
    'if not env_bool("BEAR_AGENT_REQUIRE_AUTH", True):',
    'if not env_bool("TEDDY_AGENT_REQUIRE_AUTH", env_bool("BEAR_AGENT_REQUIRE_AUTH", True)):',
)
text = text.replace(
    'legacy = os.getenv("BEAR_AGENT_TOKEN", "").strip()',
    'legacy = env_value("TEDDY_AGENT_TOKEN", "BEAR_AGENT_TOKEN", "")',
)
text = text.replace(
    'endpoint = os.getenv("BEAR_LOCAL_MODEL_URL", "http://127.0.0.1:18080/v1/chat/completions").strip()',
    'endpoint = env_value("TEDDY_LOCAL_MODEL_URL", "BEAR_LOCAL_MODEL_URL", "http://127.0.0.1:18080/v1/chat/completions")',
)
text = text.replace(
    'timeout_seconds = min(max(int(os.getenv("BEAR_LOCAL_MODEL_TIMEOUT", "60")), 20), 180)',
    'timeout_seconds = min(max(int(env_value("TEDDY_LOCAL_MODEL_TIMEOUT", "BEAR_LOCAL_MODEL_TIMEOUT", "60")), 20), 180)',
)
text = text.replace(
    'max_tokens = min(max(int(os.getenv("BEAR_LOCAL_MAX_TOKENS", "20")), 16), 24)',
    'max_tokens = min(max(int(env_value("TEDDY_LOCAL_MAX_TOKENS", "BEAR_LOCAL_MAX_TOKENS", "20")), 16), 24)',
)
main.write_text(text, encoding="utf-8")

pairing = ROOT / "server_gateway" / "app" / "pairing.py"
text = pairing.read_text(encoding="utf-8")
text = text.replace(
    '# as a practical offline guessing target. Rotate by setting BEAR_FRIEND_CODE_SHA256.',
    '# as a practical offline guessing target. Rotate with TEDDY_FRIEND_CODE_SHA256; BEAR_FRIEND_CODE_SHA256 remains a compatibility alias.',
)
text = text.replace(
    'os.getenv(\n            "BEAR_PAIRING_STORE",\n            "/home/ubuntu/.local/share/bear-agent/pairing.json",\n        )',
    'os.getenv(\n            "TEDDY_PAIRING_STORE",\n            os.getenv("BEAR_PAIRING_STORE", "/home/ubuntu/.local/share/bear-agent/pairing.json"),\n        )',
)
text = text.replace(
    'configured = os.getenv("BEAR_FRIEND_CODE_SHA256", "").strip().lower()',
    'configured = os.getenv("TEDDY_FRIEND_CODE_SHA256", os.getenv("BEAR_FRIEND_CODE_SHA256", "")).strip().lower()',
)
pairing.write_text(text, encoding="utf-8")

# Validate every JSON file we touched; this catches accidental quote breakage in
# the corpus before it reaches a fine-tuning job.
for path in (ROOT / "training").rglob("*.json"):
    json.loads(path.read_text(encoding="utf-8"))

print("TONY_TEDDY_IDENTITY_CLEANUP=PASS")
