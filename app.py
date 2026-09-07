"""Catalyst Intelligence Workspace - Chinese-first main interface."""
from __future__ import annotations

import json
from pathlib import Path

import pandas as pd
import streamlit as st

from src.catlongevity.advisor import build_recommendations
from src.catlongevity.analysis import analyze_observations
from src.catlongevity.condition_matcher import apply_condition_guard, audit_conditions
from src.catlongevity.friendly import build_user_summary
from src.catlongevity.io import observations_from_records
from src.catlongevity.reporting import render_markdown_report


st.set_page_config(page_title="催化剂智能分析平台", page_icon="⚗️", layout="wide")
st.title("催化剂智能分析平台")
st.caption("上传数据或资料，结合外部科学数据库，分析长期表现并生成可审计的中文建议。")

st.sidebar.header("功能入口")
st.sidebar.write("**长期表现分析**：当前页面")
st.sidebar.write("**资料分析**：PDF / 文本证据提取")
st.sidebar.write("**外部数据库检索**：5 个科学数据源")
st.sidebar.write("**AI 智能分析**：Analyst + Evidence Critic")
st.sidebar.info("前台尽量简单；条件匹配、删失寿命、证据来源和不确定性规则继续在后台强制执行。")

with st.expander("第一次使用？最少只需要 3 列数据", expanded=False):
    st.markdown(
        """
- **催化剂**：样品名称
- **时间**：运行时间，单位 h
- **性能**：转化率、活性、TOF 等“越高越好”的指标

建议同时填写 **温度、GHSV/WHSV、压力、进料比**。只要发现明确条件不匹配，软件会自动阻止直接优劣排名。
"""
    )


def demo_dataframe() -> pd.DataFrame:
    rows = []
    for catalyst, values in {
        "Catalyst A": [(0, 82.0), (20, 70.0), (50, 58.0)],
        "Catalyst B": [(0, 76.0), (20, 72.0), (50, 69.0)],
    }.items():
        for time_h, performance in values:
            rows.append(
                {
                    "催化剂": catalyst,
                    "时间": time_h,
                    "性能": performance,
                    "温度": 700,
                    "GHSV": 18000,
                    "进料比": "1:1",
                }
            )
    return pd.DataFrame(rows)


template_path = Path(__file__).parent / "examples" / "用户数据模板.csv"
if template_path.exists():
    st.download_button(
        "下载数据模板",
        data=template_path.read_bytes(),
        file_name="催化剂长期表现数据模板.csv",
        mime="text/csv",
    )

source_mode = st.radio("选择数据来源", ["上传文件", "直接编辑数据"], horizontal=True)
input_df: pd.DataFrame | None = None
if source_mode == "上传文件":
    uploaded = st.file_uploader("上传 CSV 或 Excel 文件", type=["csv", "xlsx", "xls"])
    if uploaded is not None:
        try:
            input_df = pd.read_csv(uploaded) if uploaded.name.lower().endswith(".csv") else pd.read_excel(uploaded)
        except Exception as exc:
            st.error(f"文件读取失败：{exc}")
else:
    st.write("可直接修改示例表，也可以整表粘贴自己的数据。")
    input_df = st.data_editor(demo_dataframe(), num_rows="dynamic", use_container_width=True)

if input_df is not None:
    st.subheader("数据预览")
    st.dataframe(input_df, use_container_width=True, hide_index=True)

    if st.button("开始分析", type="primary", use_container_width=True):
        try:
            clean_df = input_df.dropna(how="all").copy()
            records = clean_df.where(pd.notna(clean_df), "").to_dict(orient="records")
            condition_audit = audit_conditions(records)
            observations = observations_from_records(records)
            report = apply_condition_guard(analyze_observations(observations), condition_audit)
            summary = build_user_summary(report)
            recommendations = build_recommendations(report, summary)
        except Exception as exc:
            st.error(f"无法完成分析：{exc}")
            st.info("请检查每个催化剂是否有重复时间点，以及是否包含催化剂、时间和性能三列。")
        else:
            # Shared with the AI page. Only normalized analysis products are kept;
            # API keys and uploaded file bytes are never stored in session state.
            st.session_state["analysis_report"] = report
            st.session_state["analysis_summary"] = summary
            st.session_state["analysis_recommendations"] = recommendations
            st.session_state["analysis_records"] = records

            st.success("分析完成；结果已可在“AI 智能分析”页面继续使用。")
            c1, c2, c3, c4, c5 = st.columns(5)
            c1.metric("催化剂数量", summary["catalyst_count"])
            c2.metric("数据点", summary["total_observations"])
            c3.metric("最长测试时间", f"{summary['max_time_h']:g} h" if summary["max_time_h"] is not None else "—")
            c4.metric("领先顺序变化", summary["observed_rank_changes"])
            c5.metric(
                "最新共同时间领先者",
                summary["latest_shared_leader"] or "—",
                help=f"比较时间：{summary['latest_shared_time_h']:g} h" if summary["latest_shared_time_h"] is not None else None,
            )

            status = condition_audit.get("status")
            if status == "mismatch_detected":
                st.error("发现实验条件不匹配：相关催化剂对的直接排名和反超结论已自动禁用。")
            elif status == "matched_on_provided_conditions":
                st.success("在已提供的实验条件字段上未发现明显不匹配。")
            else:
                st.warning("未提供完整实验条件。结果可作描述性比较，但关键选材前建议补充实验条件。")

            tabs = st.tabs(["核心结论", "智能建议", "实验条件", "排名变化", "性能曲线", "寿命信息", "下载报告", "技术详情"])
            cards = pd.DataFrame(summary["catalyst_cards"])
            display_cards = cards.rename(
                columns={
                    "catalyst_id": "催化剂",
                    "initial_performance": "初始性能",
                    "latest_performance": "最新性能",
                    "latest_time_h": "最新时间(h)",
                    "retention_percent": "性能保持率(%)",
                    "t95_text": "95%保持时间",
                    "t90_text": "90%保持时间",
                    "t80_text": "80%保持时间",
                }
            ) if not cards.empty else cards

            with tabs[0]:
                st.subheader("快速结论")
                for conclusion in summary["pairwise_conclusions"]:
                    st.write(f"• {conclusion}")
                if not summary["pairwise_conclusions"]:
                    st.info("当前没有可输出的催化剂排名结论。")
                if not display_cards.empty:
                    st.dataframe(display_cards, use_container_width=True, hide_index=True)

            with tabs[1]:
                for item in recommendations:
                    st.markdown(f"**{item.get('level', '建议')}｜{item.get('title', '')}**")
                    st.write(item.get("text", ""))

            with tabs[2]:
                st.write(condition_audit.get("message", "未生成条件审计。"))
                condition_rows = []
                for catalyst, fields in condition_audit.get("catalysts", {}).items():
                    row = {"催化剂": catalyst}
                    row.update({field: ", ".join(values) for field, values in fields.items()})
                    condition_rows.append(row)
                if condition_rows:
                    st.dataframe(pd.DataFrame(condition_rows), use_container_width=True, hide_index=True)
                for pair, fields in condition_audit.get("pair_mismatches", {}).items():
                    st.warning(f"{pair.replace('||', ' vs ')}：{', '.join(fields)}")

            with tabs[3]:
                if summary["ranking_snapshots"]:
                    rank_df = pd.DataFrame(
                        [{"时间 (h)": s["time_h"], "领先者": s["leader"], "完整排名": s["ranking_text"]} for s in summary["ranking_snapshots"]]
                    )
                    st.dataframe(rank_df, use_container_width=True, hide_index=True)
                    st.caption("只展示共同实际观测点；不对缺失时间做插值排名。")
                else:
                    st.info("没有足够共同观测时间。")

            with tabs[4]:
                curve_rows = []
                for catalyst_id, item in report["catalysts"].items():
                    for time_h, performance in zip(item["time_h"], item["performance"]):
                        curve_rows.append({"时间 (h)": time_h, "催化剂": catalyst_id, "性能": performance})
                curve_df = pd.DataFrame(curve_rows)
                st.line_chart(curve_df.pivot_table(index="时间 (h)", columns="催化剂", values="性能", aggfunc="first"))
                st.caption("仅连接实际观测点，不补造缺失实验值。")

            with tabs[5]:
                for card in summary["catalyst_cards"]:
                    st.markdown(f"### {card['catalyst_id']}")
                    if card["retention_percent"] is not None:
                        st.write(f"测试到 {card['latest_time_h']:g} h 时，性能保持率约 {card['retention_percent']:.1f}%。")
                    st.write(f"- {card['t95_text']}")
                    st.write(f"- {card['t90_text']}")
                    st.write(f"- {card['t80_text']}")

            with tabs[6]:
                markdown_report = render_markdown_report(report)
                if not display_cards.empty:
                    st.download_button("下载结果汇总表 (.csv)", display_cards.to_csv(index=False).encode("utf-8-sig"), "催化剂长期表现汇总.csv", "text/csv", use_container_width=True)
                recommendation_text = "\n\n".join(f"## {i['level']}｜{i['title']}\n\n{i['text']}" for i in recommendations)
                st.download_button("下载中文建议 (.md)", ("# 催化剂分析建议\n\n" + recommendation_text).encode("utf-8"), "催化剂分析建议.md", "text/markdown", use_container_width=True)
                st.download_button("下载简明分析报告 (.md)", markdown_report, "catalyst_longevity_report.md", "text/markdown", use_container_width=True)
                st.download_button("下载完整分析数据 (.json)", json.dumps(report, ensure_ascii=False, indent=2), "catalyst_longevity_report.json", "application/json", use_container_width=True)

            with tabs[7]:
                st.json({"analysis": report, "recommendations": recommendations})

st.divider()
st.caption("催化剂智能分析平台 · 中文界面优先")
