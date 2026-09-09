from __future__ import annotations

import tempfile
from pathlib import Path

import build_softcopyright_final_package as base


ORIGINAL_BUILD_MANUAL = base.build_manual
ORIGINAL_HEADING = base.h
ORIGINAL_PAGE_BREAK = base.pb


def heading_with_controlled_page_break(document, text: str, size: float = 16) -> None:
    ORIGINAL_HEADING(document, text, size)
    clean = text.strip()
    if clean == "12.1 AI 助手" or clean.startswith("附录 A"):
        document.paragraphs[-1].paragraph_format.page_break_before = True


def build_manual_with_ai_section(repo: Path, out: Path) -> None:
    """Build the formal manual with the AI screenshot under its own subsection."""
    source = repo / base.MANUAL_MD
    text = source.read_text(encoding="utf-8")
    marker = "## 13. PDF 报告"
    ai_section = """## 12.1 AI 助手\n\n进入 **AI 助手** 页面可查看已经完成人工确认、允许进入分析上下文的资料范围。\n\nAI 助手遵循以下边界：\n\n- 仅使用已经关联到明确催化剂和时间点、并经人工确认的资料；\n- 未确认、未关联或信息不完整的资料不会自动进入 AI 分析上下文；\n- AI 分析结果不会改写原始实验观测、寿命阈值或催化剂直接排名；\n- 分析记录保留资料来源和人工确认状态，便于后续复核。\n\n"""
    if marker not in text:
        raise RuntimeError("Manual section marker not found: " + marker)
    text = text.replace(marker, ai_section + marker, 1)
    text = "\n".join(
        line for line in text.splitlines()
        if not line.startswith("工程构建产物的可执行文件名")
        and line.strip() != "- 报告输出：PDF"
    )

    page_break_count = {"value": 0}

    def page_break_without_blank_appendix(document) -> None:
        page_break_count["value"] += 1
        # build_manual uses three explicit page breaks: before TOC, before body,
        # and before the appendix. The appendix is instead handled with
        # page_break_before on its heading to avoid an empty intervening page.
        if page_break_count["value"] == 3:
            return
        ORIGINAL_PAGE_BREAK(document)

    with tempfile.TemporaryDirectory() as td:
        patched = Path(td) / "软件操作说明_申报版.md"
        patched.write_text(text, encoding="utf-8")
        old_manual = base.MANUAL_MD
        old_map = dict(base.SHOT_MAP)
        old_heading = base.h
        old_page_break = base.pb
        try:
            base.MANUAL_MD = patched
            base.SHOT_MAP.pop("## 13. PDF 报告", None)
            base.SHOT_MAP["## 12.1 AI 助手"] = "08_AI助手_资料准备.png"
            base.h = heading_with_controlled_page_break
            base.pb = page_break_without_blank_appendix
            ORIGINAL_BUILD_MANUAL(repo, out)
        finally:
            base.MANUAL_MD = old_manual
            base.SHOT_MAP.clear()
            base.SHOT_MAP.update(old_map)
            base.h = old_heading
            base.pb = old_page_break


base.build_manual = build_manual_with_ai_section


if __name__ == "__main__":
    base.main()
