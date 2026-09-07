"""Document ingestion helpers for the Chinese-first intelligence workspace."""
from __future__ import annotations

import re
from io import BytesIO
from typing import Any


DOI_RE = re.compile(r"10\.\d{4,9}/[-._;()/:A-Z0-9]+", re.IGNORECASE)
TEMP_RE = re.compile(r"(?<!\d)(\d{2,4}(?:\.\d+)?)\s*(?:°\s*C|℃|deg\.?\s*C)", re.IGNORECASE)
TIME_RE = re.compile(r"(?<![\w.])(\d+(?:\.\d+)?)\s*(h|hr|hrs|hour|hours|min|mins|minute|minutes)(?!\w)", re.IGNORECASE)
CONVERSION_RE = re.compile(r"(?:CH\s*4|CH4|methane)[^\n%]{0,80}?(\d+(?:\.\d+)?)\s*%", re.IGNORECASE)


def extract_text_from_bytes(filename: str, content: bytes) -> str:
    """Extract text from PDF or UTF-8-like text files.

    PDF extraction uses pypdf when installed. Scanned image-only PDFs are
    reported as having no extractable text rather than silently invoking OCR.
    """
    suffix = filename.lower().rsplit(".", 1)[-1] if "." in filename else ""
    if suffix == "pdf":
        try:
            from pypdf import PdfReader
        except ImportError as exc:
            raise RuntimeError("缺少 PDF 解析组件，请安装 requirements-ui.txt") from exc
        reader = PdfReader(BytesIO(content))
        pages = [(page.extract_text() or "").strip() for page in reader.pages]
        text = "\n\n".join(page for page in pages if page)
        if not text.strip():
            raise ValueError("该 PDF 未提取到可读文字，可能是扫描版；当前不会自动 OCR 以避免误读")
        return text
    if suffix in {"txt", "md", "csv", "tsv"}:
        for encoding in ("utf-8-sig", "utf-8", "gb18030"):
            try:
                return content.decode(encoding)
            except UnicodeDecodeError:
                continue
        raise ValueError("无法识别文本编码")
    raise ValueError("当前资料分析支持 PDF、TXT、Markdown、CSV 和 TSV")


def extract_document_signals(text: str) -> dict[str, Any]:
    """Extract conservative, source-locatable signals from document text."""
    dois = []
    for value in DOI_RE.findall(text):
        cleaned = value.rstrip(".,;)]}")
        if cleaned not in dois:
            dois.append(cleaned)

    temperatures = sorted({float(value) for value in TEMP_RE.findall(text)})
    durations_h: set[float] = set()
    for value, unit in TIME_RE.findall(text):
        numeric = float(value)
        if unit.lower().startswith("h"):
            durations_h.add(numeric)
        else:
            durations_h.add(numeric / 60.0)

    ch4_values = [float(value) for value in CONVERSION_RE.findall(text)]
    keywords = {
        "coking": ["coke", "coking", "carbon deposition", "积碳", "结焦"],
        "sintering": ["sinter", "sintering", "烧结"],
        "stability": ["stability", "stable", "deactivation", "time-on-stream", "TOS", "稳定", "失活"],
        "regeneration": ["regeneration", "regenerated", "再生"],
    }
    detected_keywords = {
        category: [word for word in words if word.lower() in text.lower()]
        for category, words in keywords.items()
    }
    detected_keywords = {key: value for key, value in detected_keywords.items() if value}

    return {
        "dois": dois[:20],
        "temperatures_c": temperatures[:50],
        "durations_h": sorted(durations_h)[:100],
        "ch4_conversion_percent_candidates": ch4_values[:100],
        "keyword_evidence": detected_keywords,
        "character_count": len(text),
    }


def evidence_snippets(text: str, terms: list[str], *, radius: int = 140, limit: int = 12) -> list[dict[str, str]]:
    """Return short source snippets around user-selected terms for auditability."""
    lower = text.lower()
    snippets: list[dict[str, str]] = []
    seen: set[tuple[str, int]] = set()
    for term in terms:
        start = 0
        target = term.lower().strip()
        if not target:
            continue
        while len(snippets) < limit:
            index = lower.find(target, start)
            if index < 0:
                break
            key = (target, index)
            if key not in seen:
                seen.add(key)
                left = max(0, index - radius)
                right = min(len(text), index + len(term) + radius)
                snippets.append({"term": term, "snippet": " ".join(text[left:right].split())})
            start = index + len(target)
    return snippets
