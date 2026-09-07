"""中文版资料分析入口。"""
from __future__ import annotations

import hashlib
import json

import pandas as pd
import streamlit as st

from src.catlongevity.advisor import build_document_advice
from src.catlongevity.documents import evidence_snippets, extract_document_signals, extract_text_from_bytes
from src.catlongevity.evidence import build_document_evidence_graph
from src.catlongevity.external_databases import lookup_crossref_doi
from src.catlongevity.ui import (
    apply_global_styles,
    empty_state,
    render_hero,
    render_sidebar,
    render_workflow,
    section_title,
    status_box,
)


st.set_page_config(page_title="资料分析｜催化剂智能分析平台", page_icon="📄", layout="wide", initial_sidebar_state="expanded")
apply_global_styles()
render_sidebar("documents")
render_hero(
    "资料分析",
    "上传论文、报告或实验说明，系统先定位 DOI、条件、稳定性相关片段与候选数值，再把可追溯证据送入后续分析。",
    eyebrow="STEP 02 · Document Intelligence",
)
render_workflow("documents")

section_title("导入资料", "支持 PDF、TXT、Markdown、CSV、TSV")
left, right = st.columns([1.5, 1], gap="large")
text = ""
filename = "粘贴文本"

with left:
    with st.container(border=True):
        source_mode = st.radio("资料来源", ["上传文件", "粘贴文本"], horizontal=True)
        if source_mode == "上传文件":
            uploaded = st.file_uploader("拖入论文或报告", type=["pdf", "txt", "md", "csv", "tsv"])
            if uploaded is not None:
                filename = uploaded.name
                try:
                    text = extract_text_from_bytes(uploaded.name, uploaded.getvalue())
                except Exception as exc:
                    status_box("资料读取失败", str(exc), tone="danger")
        else:
            text = st.text_area(
                "粘贴论文摘要、实验部分、稳定性描述或其他资料",
                height=260,
                placeholder="例如：The catalyst was tested at 700 °C for 100 h...",
            )

with right:
    with st.container(border=True):
        st.markdown("#### 系统会做什么")
        st.write("1. 识别 DOI、温度、时间等明确字段")
        st.write("2. 定位 stability / deactivation / coke / sintering 相关片段")
        st.write("3. 把转化率等数字标记为候选值，而不是自动认定为实验事实")
        st.write("4. 生成 Evidence Graph，供 AI Analyst 后续使用")
        st.caption("候选数值只有绑定到具体催化剂 + 时间 + 条件后，才能进入排名和寿命分析。")

if not text.strip():
    empty_state("等待资料", "上传一篇论文、报告，或直接粘贴文本后开始。")
else:
    source_key = hashlib.sha256((filename + "\n" + text[:5000]).encode("utf-8", errors="ignore")).hexdigest()
    if st.session_state.get("document_source_key") != source_key:
        st.session_state["document_source_key"] = source_key
        st.session_state.pop("document_crossref", None)
        st.session_state.pop("document_evidence_graph", None)
        st.session_state.pop("audited_ai_result", None)

    signals = extract_document_signals(text)
    st.session_state["document_signals"] = signals
    st.session_state["document_filename"] = filename

    section_title("识别摘要", filename)
    c1, c2, c3, c4 = st.columns(4)
    c1.metric("DOI", len(signals["dois"]))
    c2.metric("温度条件", len(signals["temperatures_c"]))
    c3.metric("时间信息", len(signals["durations_h"]))
    c4.metric("CH₄ 转化率候选", len(signals["ch4_conversion_percent_candidates"]))

    tabs = st.tabs(["结构化信息", "论文身份", "证据片段", "下一步建议", "证据图谱", "原文"])

    with tabs[0]:
        rows = []
        for value in signals["temperatures_c"]:
            rows.append({"类别": "温度", "识别值": f"{value:g} °C", "证据状态": "资料中明确出现"})
        for value in signals["durations_h"]:
            rows.append({"类别": "时间", "识别值": f"{value:g} h", "证据状态": "资料中明确出现"})
        for value in signals["ch4_conversion_percent_candidates"]:
            rows.append({"类别": "CH₄ 转化率", "识别值": f"{value:g}%", "证据状态": "候选值，尚未绑定条件"})
        for category, words in signals["keyword_evidence"].items():
            rows.append({"类别": "稳定性/机制词", "识别值": f"{category}: {', '.join(words)}", "证据状态": "关键词证据"})
        if rows:
            st.dataframe(pd.DataFrame(rows), use_container_width=True, hide_index=True)
        else:
            empty_state("没有识别到结构化信息", "可尝试上传实验结果页、稳定性章节或包含图表说明的文本。")

        if signals["ch4_conversion_percent_candidates"]:
            status_box(
                "存在未绑定的性能候选值",
                "这些数字暂时不会进入寿命或排名计算。需要先确认它属于哪个催化剂、哪个时间点和哪组实验条件。",
                tone="warning",
            )

    with tabs[1]:
        section_title("Crossref 身份校验")
        doi_default = signals["dois"][0] if signals["dois"] else ""
        doi_col, button_col = st.columns([3, 1])
        with doi_col:
            doi = st.text_input("DOI", value=doi_default, placeholder="例如 10.1039/C9CY02093D", label_visibility="collapsed")
        with button_col:
            verify = st.button("校验论文", type="primary", use_container_width=True)
        if verify and doi.strip():
            try:
                st.session_state["document_crossref"] = lookup_crossref_doi(doi)
            except Exception as exc:
                status_box("Crossref 查询失败", str(exc), tone="danger")

        crossref_metadata = st.session_state.get("document_crossref")
        if crossref_metadata:
            with st.container(border=True):
                st.markdown(f"### {crossref_metadata.get('title') or '论文'}")
                m1, m2 = st.columns(2)
                with m1:
                    st.write(f"**期刊**：{crossref_metadata.get('journal') or '未提供'}")
                    st.write(f"**年份**：{crossref_metadata.get('year') or '未提供'}")
                with m2:
                    st.write(f"**DOI**：{crossref_metadata.get('doi') or doi}")
                    authors = ", ".join((crossref_metadata.get("authors") or [])[:6]) or "未提供"
                    st.write(f"**作者**：{authors}")
                st.caption("Crossref 只用于身份与元数据校验，不替代论文中的实验数据。")
        else:
            empty_state("尚未校验论文身份", "如果资料里识别到了 DOI，可以直接点击“校验论文”。")

    with tabs[2]:
        section_title("定位可核查片段")
        default_terms = ["stability", "deactivation", "time-on-stream", "CH4", "coke", "sintering"]
        terms_text = st.text_input("关键词", value=", ".join(default_terms), help="用英文逗号分隔多个关键词。")
        terms = [item.strip() for item in terms_text.split(",") if item.strip()]
        snippets = evidence_snippets(text, terms)
        if snippets:
            for item in snippets:
                with st.container(border=True):
                    st.caption(item["term"])
                    st.write(item["snippet"])
        else:
            empty_state("没有找到对应片段", "尝试更换关键词，例如 conversion、24 h、carbon、TEM、TGA。")

    with tabs[3]:
        section_title("下一步建议")
        advice_items = build_document_advice(signals)
        for index, advice in enumerate(advice_items, start=1):
            status_box(f"建议 {index}", advice)
        st.page_link("pages/2_外部数据库检索.py", label="继续补充外部证据 →", use_container_width=True)
        st.caption("推荐顺序：身份校验 → 定位 TOS 图/表 → 绑定催化剂/时间/条件 → 再进入长期表现或 AI 分析。")

    with tabs[4]:
        graph = build_document_evidence_graph(filename, signals, st.session_state.get("document_crossref"))
        graph_dict = graph.to_dict()
        st.session_state["document_evidence_graph"] = graph_dict
        node_rows = []
        for node in graph_dict["nodes"]:
            node_rows.append(
                {
                    "节点": node["node_id"],
                    "类型": node["kind"],
                    "内容": node["label"],
                    "值": json.dumps(node.get("value"), ensure_ascii=False) if isinstance(node.get("value"), (dict, list)) else node.get("value"),
                    "来源": node["source"],
                    "证据状态": node["confidence"],
                }
            )
        st.dataframe(pd.DataFrame(node_rows), use_container_width=True, hide_index=True)
        d1, d2 = st.columns([1, 2])
        with d1:
            st.download_button(
                "下载证据图谱 JSON",
                data=json.dumps(graph_dict, ensure_ascii=False, indent=2),
                file_name="证据图谱.json",
                mime="application/json",
                use_container_width=True,
            )
        with d2:
            st.caption("证据图谱保存“资料 → 条件/候选数值 → 外部元数据”的关系，并可直接进入 AI Analyst。")

    with tabs[5]:
        section_title("已提取文本", "用于人工核对")
        st.text_area("原文", value=text[:100000], height=600, disabled=True, label_visibility="collapsed")

st.divider()
st.caption("Document Intelligence · 候选值不等于已验证实验事实")
