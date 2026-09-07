"""Shared Streamlit design system for the Chinese-first product interface."""
from __future__ import annotations

from html import escape
from typing import Iterable

import streamlit as st


PAGE_LABELS = {
    "analysis": ("01", "长期表现分析", "app.py", "分析实验数据与寿命/排名"),
    "documents": ("02", "资料分析", "pages/1_资料分析.py", "读取 PDF 与文本证据"),
    "databases": ("03", "外部数据库", "pages/2_外部数据库检索.py", "补充文献、材料和分子背景"),
    "ai": ("04", "AI 智能分析", "pages/3_AI智能分析.py", "Analyst + Evidence Critic"),
}


def apply_global_styles() -> None:
    """Apply a compact scientific-product visual system without extra dependencies."""
    st.markdown(
        """
<style>
:root {
  --ciw-bg: #f6f8fb;
  --ciw-surface: #ffffff;
  --ciw-ink: #152033;
  --ciw-muted: #667085;
  --ciw-line: #e4e9f0;
  --ciw-accent: #176b87;
  --ciw-accent-soft: #eaf5f8;
  --ciw-success: #18794e;
  --ciw-warning: #a15c00;
  --ciw-danger: #b42318;
}

[data-testid="stAppViewContainer"] {
  background: linear-gradient(180deg, #fbfcfe 0%, var(--ciw-bg) 100%);
}

[data-testid="stHeader"] {
  background: rgba(251, 252, 254, 0.82);
  backdrop-filter: blur(12px);
}

.block-container {
  max-width: 1240px;
  padding-top: 2rem;
  padding-bottom: 4rem;
}

[data-testid="stSidebar"] {
  border-right: 1px solid var(--ciw-line);
  background: #fbfcfe;
}

[data-testid="stSidebar"] .block-container {
  padding-top: 1.2rem;
}

.ciw-brand {
  display: flex;
  align-items: center;
  gap: .75rem;
  padding: .25rem 0 1.1rem 0;
}
.ciw-brand-mark {
  width: 38px;
  height: 38px;
  display: grid;
  place-items: center;
  border-radius: 11px;
  background: linear-gradient(135deg, #0f5f78, #2b8ba4);
  color: white;
  font-weight: 800;
  letter-spacing: -.02em;
  box-shadow: 0 7px 20px rgba(23, 107, 135, .18);
}
.ciw-brand-name { font-weight: 760; color: var(--ciw-ink); line-height: 1.15; }
.ciw-brand-sub { font-size: .75rem; color: var(--ciw-muted); margin-top: .12rem; }

.ciw-hero {
  border: 1px solid var(--ciw-line);
  border-radius: 20px;
  padding: 1.55rem 1.7rem;
  margin-bottom: 1.1rem;
  background:
    radial-gradient(circle at 88% 8%, rgba(36, 139, 164, .12), transparent 28%),
    linear-gradient(135deg, #ffffff 0%, #f7fbfc 100%);
  box-shadow: 0 10px 30px rgba(16, 24, 40, .045);
}
.ciw-eyebrow {
  color: var(--ciw-accent);
  text-transform: uppercase;
  letter-spacing: .1em;
  font-size: .73rem;
  font-weight: 800;
  margin-bottom: .45rem;
}
.ciw-hero h1 {
  margin: 0;
  color: var(--ciw-ink);
  font-size: clamp(1.65rem, 3vw, 2.35rem);
  line-height: 1.15;
  letter-spacing: -.035em;
}
.ciw-hero p {
  margin: .55rem 0 0 0;
  color: var(--ciw-muted);
  font-size: .98rem;
  max-width: 850px;
  line-height: 1.65;
}

.ciw-stepper {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: .55rem;
  margin: .8rem 0 1.35rem 0;
}
.ciw-step {
  border: 1px solid var(--ciw-line);
  background: rgba(255,255,255,.72);
  border-radius: 12px;
  padding: .65rem .75rem;
  min-height: 62px;
}
.ciw-step.active {
  border-color: rgba(23,107,135,.45);
  background: var(--ciw-accent-soft);
  box-shadow: inset 0 0 0 1px rgba(23,107,135,.08);
}
.ciw-step.done { background: #f4fbf7; }
.ciw-step-num { font-size: .68rem; color: var(--ciw-muted); font-weight: 800; letter-spacing: .06em; }
.ciw-step-title { font-size: .88rem; color: var(--ciw-ink); font-weight: 720; margin-top: .12rem; }
.ciw-step-state { font-size: .72rem; color: var(--ciw-muted); margin-top: .1rem; }

.ciw-section-title {
  margin: 1.45rem 0 .65rem 0;
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 1rem;
}
.ciw-section-title h2 { margin: 0; font-size: 1.1rem; color: var(--ciw-ink); letter-spacing: -.01em; }
.ciw-section-title span { color: var(--ciw-muted); font-size: .8rem; }

.ciw-status {
  border: 1px solid var(--ciw-line);
  border-left: 4px solid var(--ciw-accent);
  border-radius: 12px;
  padding: .75rem .9rem;
  background: var(--ciw-surface);
  margin: .55rem 0;
}
.ciw-status.success { border-left-color: var(--ciw-success); background: #f5fbf7; }
.ciw-status.warning { border-left-color: var(--ciw-warning); background: #fffaf2; }
.ciw-status.danger { border-left-color: var(--ciw-danger); background: #fff7f6; }
.ciw-status strong { color: var(--ciw-ink); }
.ciw-status div { color: var(--ciw-muted); margin-top: .12rem; line-height: 1.5; }

.ciw-empty {
  border: 1px dashed #cbd5e1;
  border-radius: 14px;
  padding: 1.2rem;
  text-align: center;
  color: var(--ciw-muted);
  background: rgba(255,255,255,.55);
}
.ciw-empty strong { display: block; color: var(--ciw-ink); margin-bottom: .25rem; }

[data-testid="stMetric"] {
  background: rgba(255,255,255,.88);
  border: 1px solid var(--ciw-line);
  padding: .8rem .9rem;
  border-radius: 14px;
  box-shadow: 0 4px 15px rgba(16,24,40,.025);
}
[data-testid="stMetricLabel"] { color: var(--ciw-muted); }
[data-testid="stMetricValue"] { color: var(--ciw-ink); }

.stButton > button, .stDownloadButton > button {
  border-radius: 10px;
  font-weight: 680;
  min-height: 2.55rem;
}
.stButton > button[kind="primary"] {
  box-shadow: 0 5px 15px rgba(23,107,135,.13);
}

[data-testid="stFileUploader"] {
  border-radius: 14px;
}

[data-baseweb="tab-list"] {
  gap: .3rem;
  border-bottom: 1px solid var(--ciw-line);
}
button[data-baseweb="tab"] {
  border-radius: 9px 9px 0 0;
  padding-left: .9rem;
  padding-right: .9rem;
}

[data-testid="stDataFrame"] {
  border: 1px solid var(--ciw-line);
  border-radius: 12px;
  overflow: hidden;
}

hr { border-color: var(--ciw-line); }

@media (max-width: 900px) {
  .ciw-stepper { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  .block-container { padding-left: 1rem; padding-right: 1rem; }
}
</style>
        """,
        unsafe_allow_html=True,
    )


def render_sidebar(active_page: str) -> None:
    """Render consistent product navigation and session readiness in the sidebar."""
    st.sidebar.markdown(
        """
<div class="ciw-brand">
  <div class="ciw-brand-mark">CI</div>
  <div>
    <div class="ciw-brand-name">催化剂智能分析平台</div>
    <div class="ciw-brand-sub">Catalyst Intelligence Workspace</div>
  </div>
</div>
        """,
        unsafe_allow_html=True,
    )
    for key, (number, label, path, description) in PAGE_LABELS.items():
        st.sidebar.page_link(path, label=f"{number}  {label}", icon=None, disabled=False)
        if key == active_page:
            st.sidebar.caption(description)

    st.sidebar.divider()
    st.sidebar.caption("当前工作区")
    analysis_ready = bool(st.session_state.get("analysis_report"))
    document_ready = bool(st.session_state.get("document_evidence_graph"))
    external_count = len(st.session_state.get("external_records", []))
    ai_ready = bool(st.session_state.get("audited_ai_result"))
    st.sidebar.markdown(
        f"""
- {'✓' if analysis_ready else '○'} 数据分析
- {'✓' if document_ready else '○'} 资料证据
- {'✓' if external_count else '○'} 外部证据 `{external_count}`
- {'✓' if ai_ready else '○'} AI 审查
        """
    )
    st.sidebar.caption("外部数据库与 AI 结果不会覆盖原始实验证据。")


def render_hero(title: str, subtitle: str, *, eyebrow: str) -> None:
    st.markdown(
        f"""
<div class="ciw-hero">
  <div class="ciw-eyebrow">{escape(eyebrow)}</div>
  <h1>{escape(title)}</h1>
  <p>{escape(subtitle)}</p>
</div>
        """,
        unsafe_allow_html=True,
    )


def render_workflow(active_page: str) -> None:
    """Show the four-step product journey and current session completion state."""
    completion = {
        "analysis": bool(st.session_state.get("analysis_report")),
        "documents": bool(st.session_state.get("document_evidence_graph")),
        "databases": bool(st.session_state.get("external_records")),
        "ai": bool(st.session_state.get("audited_ai_result")),
    }
    items = []
    for key, (number, label, _path, _description) in PAGE_LABELS.items():
        cls = "active" if key == active_page else ("done" if completion[key] else "")
        state = "当前步骤" if key == active_page else ("已完成" if completion[key] else "待处理")
        items.append(
            f'<div class="ciw-step {cls}"><div class="ciw-step-num">STEP {number}</div>'
            f'<div class="ciw-step-title">{escape(label)}</div><div class="ciw-step-state">{escape(state)}</div></div>'
        )
    st.markdown('<div class="ciw-stepper">' + ''.join(items) + '</div>', unsafe_allow_html=True)


def section_title(title: str, hint: str | None = None) -> None:
    hint_html = f"<span>{escape(hint)}</span>" if hint else ""
    st.markdown(
        f'<div class="ciw-section-title"><h2>{escape(title)}</h2>{hint_html}</div>',
        unsafe_allow_html=True,
    )


def status_box(title: str, text: str, *, tone: str = "neutral") -> None:
    css_tone = tone if tone in {"success", "warning", "danger"} else ""
    st.markdown(
        f'<div class="ciw-status {css_tone}"><strong>{escape(title)}</strong><div>{escape(text)}</div></div>',
        unsafe_allow_html=True,
    )


def empty_state(title: str, text: str) -> None:
    st.markdown(
        f'<div class="ciw-empty"><strong>{escape(title)}</strong>{escape(text)}</div>',
        unsafe_allow_html=True,
    )


def render_evidence_chips(labels: Iterable[str]) -> None:
    chips = " ".join(f"`{label}`" for label in labels)
    st.caption(chips)
