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
from src.catlongevity.ui import (
    apply_global_styles,
    empty_state,
    render_hero,
    render_sidebar,
    render_workflow,
    section_title,
    status_box,
)


st.set_page_config(page_title="催化剂智能分析平台", page_icon="⚗️", layout="wide", initial_sidebar_state="expanded")
apply_global_styles()
render_sidebar("analysis")
render_hero(
    "长期表现分析",
    "上传实验数据，快速比较催化剂的性能保持、寿命证据与排名变化。系统会先检查实验条件是否可比，再输出中文结论和下一步建议。",
    eyebrow="Catalyst Intelligence Workspace · 中文版",
)
render_workflow("analysis")


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


section_title("准备数据", "最少只需要：催化剂、时间、性能")
left, right = st.columns([1.55, 1], gap="large")

with left:
    with st.container(border=True):
        source_mode = st.radio(
            "数据来源",
            ["上传 CSV / Excel", "直接编辑示例数据"],
            horizontal=True,
            label_visibility="visible",
        )
        input_df: pd.DataFrame | None = None
        if source_mode == "上传 CSV / Excel":
            uploaded = st.file_uploader(
                "拖入数据文件",
                type=["csv", "xlsx", "xls"],
                help="支持 CSV、XLSX、XLS。上传文件只用于当前分析。",
            )
            if uploaded is not None:
                try:
                    input_df = pd.read_csv(uploaded) if uploaded.name.lower().endswith(".csv") else pd.read_excel(uploaded)
                except Exception as exc:
                    st.error(f"文件读取失败：{exc}")
        else:
            st.caption("可以直接改单元格、增加行，或从 Excel 整表复制粘贴。")
            input_df = st.data_editor(
                demo_dataframe(),
                num_rows="dynamic",
                use_container_width=True,
                hide_index=True,
                key="main_data_editor",
            )

with right:
    with st.container(border=True):
        st.markdown("#### 数据格式")
        st.write("**必填**：催化剂、时间、性能")
        st.write("**建议填写**：温度、GHSV/WHSV、压力、进料比")
        st.caption("一旦发现明确条件不匹配，相关催化剂对的直接排名会自动禁用。")
        template_path = Path(__file__).parent / "examples" / "用户数据模板.csv"
        if template_path.exists():
            st.download_button(
                "下载中文数据模板",
                data=template_path.read_bytes(),
                file_name="催化剂长期表现数据模板.csv",
                mime="text/csv",
                use_container_width=True,
            )
        with st.expander("支持哪些列名？"):
            st.write("催化剂：`催化剂 / 样品 / catalyst / sample`")
            st.write("时间：`时间 / TOS / time / time_h`")
            st.write("性能：`性能 / 转化率 / 活性 / conversion / activity`")
            st.write("误差范围可额外提供 `lower / upper`。")

if input_df is None:
    empty_state("等待数据", "上传实验表格，或切换到“直接编辑示例数据”即可开始。")
else:
    clean_preview = input_df.dropna(how="all")
    p1, p2, p3 = st.columns(3)
    p1.metric("当前行数", len(clean_preview))
    p2.metric("当前列数", len(clean_preview.columns))
    likely_catalyst_col = next((c for c in clean_preview.columns if str(c).strip().casefold() in {"催化剂", "样品", "catalyst", "catalyst_id", "sample"}), None)
    p3.metric("样品数量", clean_preview[likely_catalyst_col].nunique() if likely_catalyst_col is not None else "待识别")

    with st.expander("查看原始数据", expanded=False):
        st.dataframe(clean_preview, use_container_width=True, hide_index=True)

    analyze = st.button("开始智能分析", type="primary", use_container_width=True)
    if analyze:
        try:
            clean_df = input_df.dropna(how="all").copy()
            records = clean_df.where(pd.notna(clean_df), "").to_dict(orient="records")
            condition_audit = audit_conditions(records)
            observations = observations_from_records(records)
            report = apply_condition_guard(analyze_observations(observations), condition_audit)
            summary = build_user_summary(report)
            recommendations = build_recommendations(report, summary)
        except Exception as exc:
            status_box("分析未完成", str(exc), tone="danger")
            st.caption("请检查是否存在重复时间点，以及是否至少包含催化剂、时间和性能三类信息。")
        else:
            st.session_state["analysis_report"] = report
            st.session_state["analysis_summary"] = summary
            st.session_state["analysis_recommendations"] = recommendations
            st.session_state["analysis_records"] = records
            st.session_state.pop("audited_ai_result", None)

            status = condition_audit.get("status")
            if status == "mismatch_detected":
                status_box(
                    "已完成分析，但发现条件不匹配",
                    "相关催化剂对的直接排名和反超判断已自动禁用；仍可查看单条轨迹、寿命状态和数据质量。",
                    tone="danger",
                )
            elif status == "matched_on_provided_conditions":
                status_box("分析完成", "在用户已提供的实验条件字段上未发现明显不匹配，可以继续查看比较结果。", tone="success")
            else:
                status_box(
                    "分析完成，建议补充实验条件",
                    "当前结果适合描述性比较；关键选材前建议补充温度、空速、压力和进料比。",
                    tone="warning",
                )

            section_title("结果总览", "先看结论，再看技术细节")
            c1, c2, c3, c4, c5 = st.columns(5)
            c1.metric("催化剂", summary["catalyst_count"])
            c2.metric("数据点", summary["total_observations"])
            c3.metric("最长测试", f"{summary['max_time_h']:g} h" if summary["max_time_h"] is not None else "—")
            c4.metric("领先变化", summary["observed_rank_changes"])
            c5.metric(
                "当前领先",
                summary["latest_shared_leader"] or "—",
                help=f"共同观测时间：{summary['latest_shared_time_h']:g} h" if summary["latest_shared_time_h"] is not None else None,
            )

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

            tabs = st.tabs(["结论总览", "趋势与排名", "寿命与条件", "智能建议", "导出", "高级详情"])

            with tabs[0]:
                section_title("快速结论")
                conclusions = summary.get("pairwise_conclusions", [])
                if conclusions:
                    for index, conclusion in enumerate(conclusions, start=1):
                        with st.container(border=True):
                            st.markdown(f"**结论 {index}**")
                            st.write(conclusion)
                else:
                    empty_state("暂无可输出的比较结论", "可能是共同观测时间不足、实验条件不匹配，或目前只有一个催化剂。")

                section_title("催化剂概览")
                if not display_cards.empty:
                    st.dataframe(display_cards, use_container_width=True, hide_index=True)

                col_ai, col_hint = st.columns([1, 2])
                with col_ai:
                    st.page_link("pages/3_AI智能分析.py", label="继续到 AI 智能分析 →", use_container_width=True)
                with col_hint:
                    st.caption("AI 页面会继续使用这里的结构化结果，并由 Evidence Critic 独立复核。")

            with tabs[1]:
                left_chart, right_rank = st.columns([1.35, 1], gap="large")
                with left_chart:
                    section_title("性能随时间变化")
                    curve_rows = []
                    for catalyst_id, item in report["catalysts"].items():
                        for time_h, performance in zip(item["time_h"], item["performance"]):
                            curve_rows.append({"时间 (h)": time_h, "催化剂": catalyst_id, "性能": performance})
                    curve_df = pd.DataFrame(curve_rows)
                    st.line_chart(curve_df.pivot_table(index="时间 (h)", columns="催化剂", values="性能", aggfunc="first"))
                    st.caption("仅连接已有观测点，不补造缺失实验值。")

                with right_rank:
                    section_title("共同时间点排名")
                    if summary["ranking_snapshots"]:
                        rank_df = pd.DataFrame(
                            [{"时间 (h)": s["time_h"], "领先者": s["leader"], "完整排名": s["ranking_text"]} for s in summary["ranking_snapshots"]]
                        )
                        st.dataframe(rank_df, use_container_width=True, hide_index=True)
                        st.caption("只展示共同实际观测点，不对缺失时间进行插值排名。")
                    else:
                        empty_state("暂无排名历史", "至少需要两个催化剂共享一个实际观测时间点。")

            with tabs[2]:
                condition_col, life_col = st.columns([1, 1.15], gap="large")
                with condition_col:
                    section_title("实验条件可比性")
                    st.write(condition_audit.get("message", "未生成条件审计。"))
                    condition_rows = []
                    for catalyst, fields in condition_audit.get("catalysts", {}).items():
                        row = {"催化剂": catalyst}
                        row.update({field: ", ".join(values) for field, values in fields.items()})
                        condition_rows.append(row)
                    if condition_rows:
                        st.dataframe(pd.DataFrame(condition_rows), use_container_width=True, hide_index=True)
                    for pair, fields in condition_audit.get("pair_mismatches", {}).items():
                        status_box(pair.replace("||", " vs "), "不匹配字段：" + ", ".join(fields), tone="danger")

                with life_col:
                    section_title("寿命与保持状态")
                    for card in summary["catalyst_cards"]:
                        with st.expander(card["catalyst_id"], expanded=len(summary["catalyst_cards"]) <= 2):
                            if card["retention_percent"] is not None:
                                st.metric("当前性能保持率", f"{card['retention_percent']:.1f}%")
                                st.caption(f"最新观测：{card['latest_time_h']:g} h")
                            st.write(card["t95_text"])
                            st.write(card["t90_text"])
                            st.write(card["t80_text"])

            with tabs[3]:
                section_title("系统建议", "根据现有证据生成，不填补缺失事实")
                if recommendations:
                    for item in recommendations:
                        level = item.get("level", "建议")
                        tone = "warning" if level == "关键发现" else ("danger" if level == "可比性" else "neutral")
                        status_box(f"{level}｜{item.get('title', '')}", item.get("text", ""), tone=tone)
                else:
                    empty_state("暂无建议", "当前输入不足以生成进一步操作建议。")

            with tabs[4]:
                section_title("导出结果")
                markdown_report = render_markdown_report(report)
                recommendation_text = "\n\n".join(f"## {i['level']}｜{i['title']}\n\n{i['text']}" for i in recommendations)
                d1, d2 = st.columns(2)
                with d1:
                    if not display_cards.empty:
                        st.download_button(
                            "结果汇总表 · CSV",
                            display_cards.to_csv(index=False).encode("utf-8-sig"),
                            "催化剂长期表现汇总.csv",
                            "text/csv",
                            use_container_width=True,
                        )
                    st.download_button(
                        "中文建议 · Markdown",
                        ("# 催化剂分析建议\n\n" + recommendation_text).encode("utf-8"),
                        "催化剂分析建议.md",
                        "text/markdown",
                        use_container_width=True,
                    )
                with d2:
                    st.download_button(
                        "简明分析报告 · Markdown",
                        markdown_report,
                        "catalyst_longevity_report.md",
                        "text/markdown",
                        use_container_width=True,
                    )
                    st.download_button(
                        "完整结构化数据 · JSON",
                        json.dumps(report, ensure_ascii=False, indent=2),
                        "catalyst_longevity_report.json",
                        "application/json",
                        use_container_width=True,
                    )
                st.caption("导出内容保留删失状态、条件审计和来源信息；不会把未观测寿命转换成虚假的精确值。")

            with tabs[5]:
                section_title("高级详情", "面向需要核查计算与证据状态的用户")
                st.json({"analysis": report, "recommendations": recommendations})

st.divider()
st.caption("Catalyst Intelligence Workspace · 中文界面优先 · Evidence-first analysis")
