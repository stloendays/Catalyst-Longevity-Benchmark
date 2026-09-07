"""中文版外部数据库检索入口。"""
from __future__ import annotations

import pandas as pd
import streamlit as st

from src.catlongevity.external_databases import lookup_crossref_doi, search_catalysis_hub, search_crossref


st.set_page_config(page_title="外部数据库检索｜催化剂智能分析平台", page_icon="🌐", layout="wide")
st.title("外部数据库检索")
st.caption("把外部数据库当作补充证据源，而不是把数据库结果直接替代用户自己的实验或论文数据。")

source = st.radio("选择数据库", ["Crossref 文献", "Catalysis-Hub 催化反应数据"], horizontal=True)

if source == "Crossref 文献":
    mode = st.radio("检索方式", ["按 DOI 精确查询", "按关键词搜索"], horizontal=True)
    if mode == "按 DOI 精确查询":
        doi = st.text_input("输入 DOI", placeholder="10.1016/j.cej.2022.135195")
        if st.button("查询论文", type="primary", use_container_width=True) and doi.strip():
            try:
                item = lookup_crossref_doi(doi)
            except Exception as exc:
                st.error(f"查询失败：{exc}")
            else:
                st.subheader(item.get("title") or "论文")
                st.write(f"**期刊：** {item.get('journal') or '未提供'}")
                st.write(f"**年份：** {item.get('year') or '未提供'}")
                st.write(f"**作者：** {', '.join(item.get('authors') or []) or '未提供'}")
                st.write(f"**出版社：** {item.get('publisher') or '未提供'}")
                st.write(f"**DOI：** {item.get('doi')}")
                st.metric("Crossref 被引用计数", item.get("is_referenced_by_count") or 0)
                st.caption("这里的引用计数是 Crossref 元数据字段，只作为文献背景信息，不代表论文质量或催化剂性能。")
    else:
        query = st.text_input("输入论文标题、催化剂或研究关键词", placeholder="Ni Al2O3 dry reforming deactivation")
        if st.button("搜索文献", type="primary", use_container_width=True) and query.strip():
            try:
                results = search_crossref(query)
            except Exception as exc:
                st.error(f"搜索失败：{exc}")
            else:
                if not results:
                    st.info("没有找到结果。")
                else:
                    df = pd.DataFrame(
                        [
                            {
                                "年份": item.get("year"),
                                "标题": item.get("title"),
                                "期刊": item.get("journal"),
                                "DOI": item.get("doi"),
                                "作者": ", ".join((item.get("authors") or [])[:5]),
                            }
                            for item in results
                        ]
                    )
                    st.dataframe(df, use_container_width=True, hide_index=True)
                    st.caption("搜索结果只用于发现文献。真正进入催化剂长期表现分析前，仍需读取原文中的实验条件和 TOS 数据。")

else:
    st.info("Catalysis-Hub 主要提供计算催化反应能、活化能和体系信息，适合做机理/材料背景补充；不能把计算能量直接当作长期稳定性证据。")
    c1, c2 = st.columns(2)
    with c1:
        reactants = st.text_input("Reactants", placeholder="例如 CO2")
    with c2:
        products = st.text_input("Products", placeholder="例如 CO")
    limit = st.slider("最多返回记录", 1, 25, 10)
    if st.button("检索 Catalysis-Hub", type="primary", use_container_width=True):
        if not reactants.strip() and not products.strip():
            st.warning("至少输入 Reactants 或 Products。")
        else:
            try:
                results = search_catalysis_hub(reactants=reactants, products=products, first=limit)
            except Exception as exc:
                st.error(f"检索失败：{exc}")
            else:
                if not results:
                    st.info("未找到匹配记录。")
                else:
                    df = pd.DataFrame(results).rename(
                        columns={
                            "chemical_composition": "化学组成",
                            "reaction_energy_eV": "反应能 (eV)",
                            "activation_energy_eV": "活化能 (eV)",
                            "reactants": "反应物",
                            "products": "产物",
                        }
                    )
                    st.dataframe(df, use_container_width=True, hide_index=True)
                    st.caption("这些结果属于外部计算数据库证据；后续会与用户上传资料、文献 TOS 数据和内部 longevity 分析分层保存。")

st.divider()
st.markdown("### 当前数据库接入路线")
st.write("已接入：Crossref、Catalysis-Hub。下一步计划：Semantic Scholar、Materials Project、PubChem，并统一进入 Evidence Graph。")
