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


st.set_page_config(page_title="外部数据库检索｜催化剂智能分析平台", page_icon="🌐", layout="wide")
st.title("外部数据库检索")
st.caption("用外部数据库补充文献、材料、分子和计算催化背景；所有结果与用户实验/TOS 证据分层保存。")

if "external_records" not in st.session_state:
    st.session_state["external_records"] = []
if "db_results" not in st.session_state:
    st.session_state["db_results"] = []

source = st.selectbox(
    "选择数据库",
    [
        "Crossref｜论文身份与元数据",
        "Semantic Scholar｜相关论文与引用网络",
        "Catalysis-Hub｜计算催化反应数据",
        "Materials Project｜材料结构与性质",
        "PubChem｜化合物身份与结构",
    ],
)

results: list[dict] = []

if source.startswith("Crossref"):
    mode = st.radio("检索方式", ["按 DOI 精确查询", "按关键词搜索"], horizontal=True)
    if mode == "按 DOI 精确查询":
        doi = st.text_input("输入 DOI", placeholder="10.1016/j.cej.2022.135195")
        if st.button("查询 Crossref", type="primary", use_container_width=True) and doi.strip():
            try:
                results = [lookup_crossref_doi(doi)]
                st.session_state["db_results"] = results
            except Exception as exc:
                st.error(f"查询失败：{exc}")
    else:
        query = st.text_input("论文标题、催化剂或关键词", placeholder="Ni Al2O3 dry reforming deactivation")
        if st.button("搜索 Crossref", type="primary", use_container_width=True) and query.strip():
            try:
                results = search_crossref(query)
                st.session_state["db_results"] = results
            except Exception as exc:
                st.error(f"搜索失败：{exc}")

elif source.startswith("Semantic Scholar"):
    st.info("Semantic Scholar 用于发现相关论文、引用量和开放获取入口。没有 API Key 也可尝试查询；高频使用可配置自己的 key。")
    mode = st.radio("检索方式", ["按关键词搜索", "按 DOI 查询"], horizontal=True)
    s2_key = st.text_input("Semantic Scholar API Key（可选）", type="password", help="只用于当前运行，不保存到仓库或结果文件。")
    if mode == "按关键词搜索":
        query = st.text_input("研究问题或关键词", placeholder="nickel catalyst dry reforming deactivation")
        if st.button("搜索 Semantic Scholar", type="primary", use_container_width=True) and query.strip():
            try:
                results = search_semantic_scholar(query, api_key=s2_key or None)
                st.session_state["db_results"] = results
            except Exception as exc:
                st.error(f"搜索失败：{exc}")
    else:
        doi = st.text_input("输入 DOI", placeholder="10.1039/C9CY02093D")
        if st.button("查询 Semantic Scholar", type="primary", use_container_width=True) and doi.strip():
            try:
                results = [lookup_semantic_scholar_doi(doi, api_key=s2_key or None)]
                st.session_state["db_results"] = results
            except Exception as exc:
                st.error(f"查询失败：{exc}")

elif source.startswith("Catalysis-Hub"):
    st.info("Catalysis-Hub 提供计算反应能/活化能等背景信息，不能替代实验长期稳定性证据。")
    c1, c2 = st.columns(2)
    with c1:
        reactants = st.text_input("Reactants", placeholder="CO2")
    with c2:
        products = st.text_input("Products", placeholder="CO")
    limit = st.slider("最多返回记录", 1, 25, 10)
    if st.button("检索 Catalysis-Hub", type="primary", use_container_width=True):
        if not reactants.strip() and not products.strip():
            st.warning("至少输入 Reactants 或 Products。")
        else:
            try:
                results = search_catalysis_hub(reactants=reactants, products=products, first=limit)
                st.session_state["db_results"] = results
            except Exception as exc:
                st.error(f"检索失败：{exc}")

elif source.startswith("Materials Project"):
    st.info("Materials Project 用于材料结构、稳定性、能带和密度等背景 enrichment。需要用户自己的 API Key。")
    formula = st.text_input("化学式", placeholder="CeO2 或 NiAl2O4")
    mp_key = st.text_input("Materials Project API Key", type="password", help="仅用于本次查询；不会写入仓库、日志或分析报告。")
    limit = st.slider("最多返回材料", 1, 25, 10)
    if st.button("查询 Materials Project", type="primary", use_container_width=True) and formula.strip():
        try:
            results = search_materials_project_formula(formula, api_key=mp_key or None, limit=limit)
            st.session_state["db_results"] = results
        except Exception as exc:
            st.error(f"查询失败：{exc}")

else:
    st.info("PubChem 用于规范化 precursor、promoter、溶剂和分子物种的身份；它不是催化剂寿命数据库。")
    compound = st.text_input("化合物名称", placeholder="nickel nitrate hexahydrate / cerium oxide / acetone")
    if st.button("查询 PubChem", type="primary", use_container_width=True) and compound.strip():
        try:
            results = [lookup_pubchem_compound(compound)]
            st.session_state["db_results"] = results
        except Exception as exc:
            st.error(f"查询失败：{exc}")

current = st.session_state.get("db_results", [])
if current:
    st.divider()
    st.subheader("当前检索结果")
    display = pd.DataFrame(current)
    preferred = [
        "source", "title", "year", "doi", "venue", "journal", "citation_count",
        "material_id", "formula", "is_stable", "energy_above_hull_eV_atom", "band_gap_eV",
        "cid", "molecular_formula", "molecular_weight", "chemical_composition",
        "reaction_energy_eV", "activation_energy_eV", "url",
    ]
    ordered = [col for col in preferred if col in display.columns] + [col for col in display.columns if col not in preferred]
    st.dataframe(display[ordered], use_container_width=True, hide_index=True)

    c1, c2 = st.columns(2)
    with c1:
        if st.button("加入 AI 证据篮", type="primary", use_container_width=True):
            existing = st.session_state.get("external_records", [])
            signatures = {str((item.get("source"), item.get("doi"), item.get("paper_id"), item.get("material_id"), item.get("cid"), item.get("id"))) for item in existing}
            added = 0
            for item in current:
                signature = str((item.get("source"), item.get("doi"), item.get("paper_id"), item.get("material_id"), item.get("cid"), item.get("id")))
                if signature not in signatures:
                    existing.append(item)
                    signatures.add(signature)
                    added += 1
            st.session_state["external_records"] = existing
            st.success(f"已加入 {added} 条；证据篮现有 {len(existing)} 条记录。")
    with c2:
        if st.button("清空当前结果", use_container_width=True):
            st.session_state["db_results"] = []
            st.rerun()

st.divider()
st.markdown("### AI 证据篮")
basket = st.session_state.get("external_records", [])
st.write(f"当前已选择 **{len(basket)}** 条外部记录。AI Analyst 会把它们当作背景证据，并明确禁止把它们直接当成长期稳定性真值。")
if basket:
    basket_df = pd.DataFrame(basket)
    cols = [col for col in ["source", "title", "doi", "material_id", "formula", "cid", "chemical_composition"] if col in basket_df.columns]
    st.dataframe(basket_df[cols] if cols else basket_df, use_container_width=True, hide_index=True)
    if st.button("清空 AI 证据篮"):
        st.session_state["external_records"] = []
        st.rerun()

st.caption("已接入：Crossref、Semantic Scholar、Catalysis-Hub、Materials Project、PubChem。数据库结果均保留 source 标签并进入 AI 的分层 Evidence Packet。")
