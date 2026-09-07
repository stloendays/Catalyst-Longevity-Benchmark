"""Browser interface for Catalyst Longevity Analyzer.

Run with:
    streamlit run app.py
"""
from __future__ import annotations

import json
from pathlib import Path

import pandas as pd
import streamlit as st

from src.catlongevity.analysis import analyze_observations
from src.catlongevity.friendly import build_user_summary
from src.catlongevity.io import observations_from_records
from src.catlongevity.reporting import render_markdown_report


st.set_page_config(page_title="Catalyst Longevity Analyzer", page_icon="⚗️", layout="wide")

st.title("Catalyst Longevity Analyzer")
st.caption("上传催化剂随时间变化的数据，自动比较稳定性、寿命区间和长期领先关系。")

with st.expander("第一次使用？只需要准备 3 列数据", expanded=False):
    st.markdown(
        """
最简单的数据表只需要：

- **催化剂名称**：例如 `Catalyst A`
- **时间**：单位小时，例如 `0, 10, 20, 50`
- **性能**：例如 CH4 转化率、活性或 TOF

列名可以直接使用中文，也可以使用 `catalyst_id / time_h / performance`。
如果有误差上下限，还可以增加 `lower / upper`；没有也可以正常分析。
"""
    )


def demo_dataframe() -> pd.DataFrame:
    return pd.DataFrame(
        [
            {"催化剂": "Catalyst A", "时间": 0, "性能": 82.0},
            {"催化剂": "Catalyst A", "时间": 20, "性能": 70.0},
            {"催化剂": "Catalyst A", "时间": 50, "性能": 58.0},
            {"催化剂": "Catalyst B", "时间": 0, "性能": 76.0},
            {"催化剂": "Catalyst B", "时间": 20, "性能": 72.0},
            {"催化剂": "Catalyst B", "时间": 50, "性能": 69.0},
        ]
    )


source_mode = st.radio(
    "选择数据来源",
    ["上传文件", "直接编辑数据"],
    horizontal=True,
)

input_df: pd.DataFrame | None = None

if source_mode == "上传文件":
    uploaded = st.file_uploader("上传 CSV 或 Excel 文件", type=["csv", "xlsx", "xls"])
    if uploaded is not None:
        try:
            if uploaded.name.lower().endswith(".csv"):
                input_df = pd.read_csv(uploaded)
            else:
                input_df = pd.read_excel(uploaded)
        except Exception as exc:
            st.error(f"文件读取失败：{exc}")
else:
    st.write("可以直接修改下面的示例数据，也可以删除后粘贴自己的表格。")
    input_df = st.data_editor(demo_dataframe(), num_rows="dynamic", use_container_width=True)

if input_df is not None:
    st.subheader("数据预览")
    st.dataframe(input_df, use_container_width=True, hide_index=True)

    if st.button("开始分析", type="primary", use_container_width=True):
        try:
            clean_df = input_df.dropna(how="all").copy()
            records = clean_df.where(pd.notna(clean_df), "").to_dict(orient="records")
            observations = observations_from_records(records)
            report = analyze_observations(observations)
            summary = build_user_summary(report)
        except Exception as exc:
            st.error(f"无法完成分析：{exc}")
            st.info("请检查每个催化剂是否有重复时间点，以及是否包含催化剂名称、时间和性能三列。")
        else:
            st.success("分析完成")

            c1, c2, c3, c4 = st.columns(4)
            c1.metric("催化剂数量", summary["catalyst_count"])
            c2.metric("数据点", summary["total_observations"])
            c3.metric("最长测试时间", f"{summary['max_time_h']:g} h" if summary["max_time_h"] is not None else "—")
            c4.metric("发现领先顺序变化", summary["observed_rank_changes"])

            tabs = st.tabs(["结论", "性能曲线", "寿命信息", "下载报告", "专业详情"])

            with tabs[0]:
                st.subheader("快速结论")
                if summary["pairwise_conclusions"]:
                    for conclusion in summary["pairwise_conclusions"]:
                        st.write(f"• {conclusion}")
                else:
                    st.info("当前只有一个催化剂，暂无催化剂之间的排名比较。")

                st.subheader("各催化剂概览")
                cards = pd.DataFrame(summary["catalyst_cards"])
                if not cards.empty:
                    cards = cards.rename(
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
                    )
                    st.dataframe(cards, use_container_width=True, hide_index=True)

            with tabs[1]:
                st.subheader("性能随时间变化")
                curve_rows = []
                for catalyst_id, item in report["catalysts"].items():
                    for time_h, performance in zip(item["time_h"], item["performance"]):
                        curve_rows.append({"时间 (h)": time_h, "催化剂": catalyst_id, "性能": performance})
                curve_df = pd.DataFrame(curve_rows)
                pivot = curve_df.pivot_table(index="时间 (h)", columns="催化剂", values="性能", aggfunc="first")
                st.line_chart(pivot)
                st.caption("曲线只连接已有观测点；软件不会自动补造缺失的实验数据。")

            with tabs[2]:
                st.subheader("寿命与保持情况")
                for card in summary["catalyst_cards"]:
                    st.markdown(f"### {card['catalyst_id']}")
                    if card["retention_percent"] is not None:
                        st.write(
                            f"测试到 {card['latest_time_h']:g} 小时时，性能约保留初始值的 "
                            f"{card['retention_percent']:.1f}%。"
                        )
                    st.write(f"- {card['t95_text']}")
                    st.write(f"- {card['t90_text']}")
                    st.write(f"- {card['t80_text']}")

            with tabs[3]:
                markdown_report = render_markdown_report(report)
                st.download_button(
                    "下载简明分析报告 (.md)",
                    data=markdown_report,
                    file_name="catalyst_longevity_report.md",
                    mime="text/markdown",
                    use_container_width=True,
                )
                st.download_button(
                    "下载完整分析数据 (.json)",
                    data=json.dumps(report, ensure_ascii=False, indent=2),
                    file_name="catalyst_longevity_report.json",
                    mime="application/json",
                    use_container_width=True,
                )

            with tabs[4]:
                st.caption("这一页保留给需要核查计算细节、数据来源和不确定性状态的高级用户。")
                st.json(report)

st.divider()
st.caption("Catalyst Longevity Analyzer · 长期表现比较工具")
