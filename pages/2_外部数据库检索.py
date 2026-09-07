"""中文版外部数据库检索入口。"""
from __future__ import annotations

import pandas as pd
import streamlit as st

from src.catlongevity.external_databases import (
    lookup_crossref_doi,
    lookup_pubchem_compound,
    lookup_semantic_scholar_doi,
    search_catalysis_hub,
    search_crossref,
    search_materials_project_formula,
    search_semantic_scholar,
)
from src.catlongevity.ui import (
    apply_global_styles,
    empty_state,
    render_hero,
    render_sidebar,
    render_workflow,
    section_title,
    status_box,
)


st.set_page_config(page_title="外部数据库｜催化剂智能分析平台", page_icon="🌐", layout="wide", initial_sidebar_state="expanded")
apply_global_styles()
render_sidebar("databases")
render_hero(
    "外部数据库",
    "从文献、材料、分子和计算催化数据库补充背景证据。外部记录始终与用户实验/TOS 数据分层保存，不会直接替代长期稳定性证据。",
    eyebrow="STEP 03 · External Evidence",
)
render_workflow("databases")

if "external_records" not in st.session_state:
    st.session_state["external_records"] = []
if "db_results" not in st.session_state:
    st.session_state["db_results"] = []

source_cards = {
    "Crossref": "论文身份、DOI、作者、期刊与出版元数据",
    "Semantic Scholar": "相关论文、引用网络、摘要与开放获取入口",
    "Catalysis-Hub": "计算反应能、活化能和催化体系背景",
    "Materials Project": "材料结构、稳定性、能带与密度等性质",
    "PubChem": "化合物身份、分子式、SMILES 与 InChI",
}

section_title("选择数据源", "检索结果可加入 AI 证据篮")
source = st.selectbox("数据库", list(source_cards), label_visibility="collapsed")
status_box(source, source_cards[source])

results: list[dict] = []

with st.container(border=True):
    if source == "Crossref":
        mode = st.radio("检索方式", ["按 DOI 精确查询", "按关键词搜索"], horizontal=True)
        if mode == "按 DOI 精确查询":
            query_col, button_col = st.columns([3, 1])
            with query_col:
                doi = st.text_input("DOI", placeholder="10.1016/j.cej.2022.135195", label_visibility="collapsed")
            with button_col:
                run = st.button("查询 Crossref", type="primary", use_container_width=True)
            if run and doi.strip():
                try:
                    results = [lookup_crossref_doi(doi)]
                    st.session_state["db_results"] = results
                except Exception as exc:
                    status_box("查询失败", str(exc), tone="danger")
        else:
            query = st.text_input("论文标题、催化剂或关键词", placeholder="Ni Al2O3 dry reforming deactivation")
            if st.button("搜索 Crossref", type="primary", use_container_width=True) and query.strip():
                try:
                    results = search_crossref(query)
                    st.session_state["db_results"] = results
                except Exception as exc:
                    status_box("搜索失败", str(exc), tone="danger")

    elif source == "Semantic Scholar":
        mode = st.radio("检索方式", ["按关键词搜索", "按 DOI 查询"], horizontal=True)
        with st.expander("API 设置（可选）"):
            s2_key = st.text_input(
                "Semantic Scholar API Key",
                type="password",
                help="仅用于当前运行，不保存到仓库、证据包或导出结果。",
            )
            st.caption("普通查询可尝试不填写；高频查询再配置 key。")
        if mode == "按关键词搜索":
            query = st.text_input("研究问题或关键词", placeholder="nickel catalyst dry reforming deactivation")
            if st.button("搜索 Semantic Scholar", type="primary", use_container_width=True) and query.strip():
                try:
                    results = search_semantic_scholar(query, api_key=s2_key or None)
                    st.session_state["db_results"] = results
                except Exception as exc:
                    status_box("搜索失败", str(exc), tone="danger")
        else:
            doi = st.text_input("DOI", placeholder="10.1039/C9CY02093D")
            if st.button("查询 Semantic Scholar", type="primary", use_container_width=True) and doi.strip():
                try:
                    results = [lookup_semantic_scholar_doi(doi, api_key=s2_key or None)]
                    st.session_state["db_results"] = results
                except Exception as exc:
                    status_box("查询失败", str(exc), tone="danger")

    elif source == "Catalysis-Hub":
        c1, c2 = st.columns(2)
        with c1:
            reactants = st.text_input("Reactants", placeholder="CO2")
        with c2:
            products = st.text_input("Products", placeholder="CO")
        limit = st.slider("最多返回记录", 1, 25, 10)
        if st.button("检索 Catalysis-Hub", type="primary", use_container_width=True):
            if not reactants.strip() and not products.strip():
                status_box("缺少检索条件", "至少输入 Reactants 或 Products。", tone="warning")
            else:
                try:
                    results = search_catalysis_hub(reactants=reactants, products=products, first=limit)
                    st.session_state["db_results"] = results
                except Exception as exc:
                    status_box("检索失败", str(exc), tone="danger")

    elif source == "Materials Project":
        formula = st.text_input("化学式", placeholder="CeO2 或 NiAl2O4")
        with st.expander("API 设置"):
            mp_key = st.text_input(
                "Materials Project API Key",
                type="password",
                help="只用于本次查询；不会写入仓库、日志或分析报告。",
            )
            st.caption("Materials Project 查询需要用户自己的 API Key。")
        limit = st.slider("最多返回材料", 1, 25, 10)
        if st.button("查询 Materials Project", type="primary", use_container_width=True) and formula.strip():
            try:
                results = search_materials_project_formula(formula, api_key=mp_key or None, limit=limit)
                st.session_state["db_results"] = results
            except Exception as exc:
                status_box("查询失败", str(exc), tone="danger")

    else:
        compound = st.text_input("化合物名称", placeholder="nickel nitrate hexahydrate / cerium oxide / acetone")
        if st.button("查询 PubChem", type="primary", use_container_width=True) and compound.strip():
            try:
                results = [lookup_pubchem_compound(compound)]
                st.session_state["db_results"] = results
            except Exception as exc:
                status_box("查询失败", str(exc), tone="danger")

current = st.session_state.get("db_results", [])
section_title("当前检索结果", f"{len(current)} 条")
if current:
    display = pd.DataFrame(current)
    preferred = [
        "source", "title", "year", "doi", "venue", "journal", "citation_count",
        "material_id", "formula", "is_stable", "energy_above_hull_eV_atom", "band_gap_eV",
        "cid", "molecular_formula", "molecular_weight", "chemical_composition",
        "reaction_energy_eV", "activation_energy_eV", "url",
    ]
    ordered = [col for col in preferred if col in display.columns] + [col for col in display.columns if col not in preferred]
    st.dataframe(display[ordered], use_container_width=True, hide_index=True)

    c1, c2 = st.columns([1.3, 1])
    with c1:
        if st.button("加入 AI 证据篮", type="primary", use_container_width=True):
            existing = st.session_state.get("external_records", [])
            signatures = {
                str((item.get("source"), item.get("doi"), item.get("paper_id"), item.get("material_id"), item.get("cid"), item.get("id")))
                for item in existing
            }
            added = 0
            for item in current:
                signature = str((item.get("source"), item.get("doi"), item.get("paper_id"), item.get("material_id"), item.get("cid"), item.get("id")))
                if signature not in signatures:
                    existing.append(item)
                    signatures.add(signature)
                    added += 1
            st.session_state["external_records"] = existing
            st.session_state.pop("audited_ai_result", None)
            status_box("已加入证据篮", f"新增 {added} 条，当前共 {len(existing)} 条外部记录。", tone="success")
    with c2:
        if st.button("清空当前结果", use_container_width=True):
            st.session_state["db_results"] = []
            st.rerun()
else:
    empty_state("还没有检索结果", "选择数据库并输入 DOI、关键词、化学式或分子名称后开始查询。")

section_title("AI 证据篮", "只作为背景证据，不自动成为长期稳定性真值")
basket = st.session_state.get("external_records", [])
if basket:
    b1, b2, b3 = st.columns([1, 1, 2])
    b1.metric("外部记录", len(basket))
    sources = sorted({str(item.get("source") or "Unknown") for item in basket})
    b2.metric("数据源", len(sources))
    b3.caption("已选来源：" + " · ".join(sources))

    basket_df = pd.DataFrame(basket)
    cols = [col for col in ["source", "title", "doi", "material_id", "formula", "cid", "chemical_composition"] if col in basket_df.columns]
    st.dataframe(basket_df[cols] if cols else basket_df, use_container_width=True, hide_index=True)

    a1, a2 = st.columns([1.3, 1])
    with a1:
        st.page_link("pages/3_AI智能分析.py", label="带着这些证据进入 AI 分析 →", use_container_width=True)
    with a2:
        if st.button("清空 AI 证据篮", use_container_width=True):
            st.session_state["external_records"] = []
            st.session_state.pop("audited_ai_result", None)
            st.rerun()
else:
    empty_state("证据篮为空", "在上方检索到合适记录后，点击“加入 AI 证据篮”。")

st.divider()
st.caption("External Evidence · Crossref · Semantic Scholar · Catalysis-Hub · Materials Project · PubChem")
