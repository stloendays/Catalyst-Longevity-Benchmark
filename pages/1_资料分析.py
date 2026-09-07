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


st.set_page_config(page_title="资料分析｜催化剂智能分析平台", page_icon="📄", layout="wide")
st.title("资料分析")
st.caption("上传论文、报告或文本资料，系统先提取可核查信息，再连接外部数据库补充元数据。")

st.info("当前采用保守提取：不会把文本中孤立的数字自动当成某个催化剂的实验结果。识别到的候选值必须先绑定到催化剂、时间和条件后才能进入排名分析。")

source_mode = st.radio("资料来源", ["上传文件", "粘贴文本"], horizontal=True)
text = ""
filename = "粘贴文本"

if source_mode == "上传文件":
    uploaded = st.file_uploader("上传 PDF、TXT、Markdown、CSV 或 TSV", type=["pdf", "txt", "md", "csv", "tsv"])
    if uploaded is not None:
        filename = uploaded.name
        try:
            text = extract_text_from_bytes(uploaded.name, uploaded.getvalue())
        except Exception as exc:
            st.error(f"资料读取失败：{exc}")
else:
    text = st.text_area("粘贴论文摘要、实验部分、稳定性描述或其他资料", height=260)

if text.strip():
    source_key = hashlib.sha256((filename + "\n" + text[:5000]).encode("utf-8", errors="ignore")).hexdigest()
    if st.session_state.get("document_source_key") != source_key:
        st.session_state["document_source_key"] = source_key
        st.session_state.pop("document_crossref", None)

    signals = extract_document_signals(text)

    c1, c2, c3, c4 = st.columns(4)
    c1.metric("识别 DOI", len(signals["dois"]))
    c2.metric("温度条件", len(signals["temperatures_c"]))
    c3.metric("时间信息", len(signals["durations_h"]))
    c4.metric("CH4 转化率候选", len(signals["ch4_conversion_percent_candidates"]))

    tabs = st.tabs(["关键信息", "论文身份", "证据片段", "分析建议", "证据图谱", "原文"])

    with tabs[0]:
        rows = []
        for value in signals["temperatures_c"]:
            rows.append({"类别": "温度", "识别值": f"{value:g} °C", "状态": "资料中出现"})
        for value in signals["durations_h"]:
            rows.append({"类别": "时间", "识别值": f"{value:g} h", "状态": "资料中出现"})
        for value in signals["ch4_conversion_percent_candidates"]:
            rows.append({"类别": "CH4 转化率", "识别值": f"{value:g}%", "状态": "候选值，尚未绑定条件"})
        for category, words in signals["keyword_evidence"].items():
            rows.append({"类别": "机制/稳定性词", "识别值": f"{category}: {', '.join(words)}", "状态": "关键词证据"})
        if rows:
            st.dataframe(pd.DataFrame(rows), use_container_width=True, hide_index=True)
        else:
            st.warning("暂未提取到可结构化的催化剂稳定性信息。")

    with tabs[1]:
        doi_default = signals["dois"][0] if signals["dois"] else ""
        doi = st.text_input("DOI", value=doi_default, placeholder="例如 10.1039/C9CY02093D")
        if st.button("校验论文身份", type="primary") and doi.strip():
            try:
                st.session_state["document_crossref"] = lookup_crossref_doi(doi)
            except Exception as exc:
                st.error(f"Crossref 查询失败：{exc}")
        crossref_metadata = st.session_state.get("document_crossref")
        if crossref_metadata:
            st.subheader(crossref_metadata.get("title") or "论文")
            st.write(f"**期刊：** {crossref_metadata.get('journal') or '未提供'}")
            st.write(f"**年份：** {crossref_metadata.get('year') or '未提供'}")
            st.write(f"**作者：** {', '.join(crossref_metadata.get('authors') or []) or '未提供'}")
            st.write(f"**DOI：** {crossref_metadata.get('doi') or doi}")
            st.caption("Crossref 用于论文身份和元数据校验，不替代论文中的实验数据。")

    with tabs[2]:
        default_terms = ["stability", "deactivation", "time-on-stream", "CH4", "coke", "sintering"]
        terms_text = st.text_input("定位关键词（逗号分隔）", value=", ".join(default_terms))
        terms = [item.strip() for item in terms_text.split(",") if item.strip()]
        snippets = evidence_snippets(text, terms)
        if snippets:
            for item in snippets:
                st.markdown(f"**{item['term']}**")
                st.write(item["snippet"])
                st.divider()
        else:
            st.info("没有找到这些关键词对应的片段。")

    with tabs[3]:
        st.subheader("系统建议")
        for advice in build_document_advice(signals):
            st.write(f"• {advice}")
        st.markdown("### 下一步分析入口")
        st.write("1. 先校验 DOI 和论文身份；2. 定位 TOS 图/表；3. 将催化剂—时间—性能值绑定；4. 再进入首页的长期表现分析。")

    with tabs[4]:
        graph = build_document_evidence_graph(filename, signals, st.session_state.get("document_crossref"))
        graph_dict = graph.to_dict()
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
        st.caption("证据图谱用于保存“资料 → 条件/数值 → 外部元数据”的关系，后续会继续连接分析结论与推荐。")
        st.download_button(
            "下载证据图谱 JSON",
            data=json.dumps(graph_dict, ensure_ascii=False, indent=2),
            file_name="证据图谱.json",
            mime="application/json",
        )

    with tabs[5]:
        st.text_area("已提取文本", value=text[:100000], height=600, disabled=True)
