"""中文版 AI Analyst + Evidence Critic 入口。"""
from __future__ import annotations

import json

import pandas as pd
import streamlit as st

from src.catlongevity.ai_analyst import (
    build_evidence_packet,
    deterministic_precheck,
    run_audited_ai_analysis,
)


st.set_page_config(page_title="AI 智能分析｜催化剂智能分析平台", page_icon="🧠", layout="wide")
st.title("AI 智能分析")
st.caption("AI Analyst 先综合证据提出建议，Evidence Critic 再独立审查。Critic 可以阻止不可靠的决策结论。")

report = st.session_state.get("analysis_report")
summary = st.session_state.get("analysis_summary")
recommendations = st.session_state.get("analysis_recommendations")
document_graph = st.session_state.get("document_evidence_graph")
external_records = st.session_state.get("external_records", [])

c1, c2, c3 = st.columns(3)
c1.metric("长期表现分析", "已加载" if report else "未加载")
c2.metric("资料证据图谱", "已加载" if document_graph else "未加载")
c3.metric("外部证据", len(external_records))

if not report:
    st.info("如果你要比较催化剂长期表现，请先在首页完成一次数据分析。AI 也可以只基于资料证据做“证据缺口/下一步建议”，但不会凭空生成催化剂排名。")

packet = build_evidence_packet(
    report=report,
    summary=summary,
    recommendations=recommendations,
    evidence_graph=document_graph,
    external_records=external_records,
)

with st.expander("查看 AI 将收到哪些证据", expanded=False):
    evidence_rows = []
    for item in packet.get("evidence", []):
        data = item.get("data") or {}
        label = None
        if isinstance(data, dict):
            label = data.get("label") or data.get("title") or data.get("catalyst_id") or data.get("source")
        evidence_rows.append({"Evidence ID": item.get("id"), "类型": item.get("type"), "摘要": label or "结构化数据"})
    if evidence_rows:
        st.dataframe(pd.DataFrame(evidence_rows), use_container_width=True, hide_index=True)
    else:
        st.warning("当前没有证据。请先分析数据、上传资料或从外部数据库加入证据篮。")

precheck = deterministic_precheck(packet)
if precheck:
    st.error("确定性规则已经发现硬性问题。AI 可以帮助解释问题，但不能绕过这些规则。")
    for issue in precheck:
        st.write(f"- {issue['issue']}（{issue['evidence_id']}）")

question = st.text_area(
    "你希望 AI 回答什么？",
    value="结合当前实验数据、资料和外部背景，哪个催化剂更适合更长时间运行？现有证据能支持到什么程度，还需要补什么数据？",
    height=120,
)

with st.expander("AI 设置", expanded=False):
    model = st.selectbox("模型", ["gpt-5.6-terra", "gpt-5.6-sol", "gpt-5.6-luna"], index=0)
    api_key = st.text_input("OpenAI API Key", type="password", help="只用于当前调用，不写入仓库、证据包或分析报告。也可以通过 OPENAI_API_KEY 环境变量提供。")
    st.caption("默认使用 Terra 平衡分析能力与成本；复杂证据审查可切换 Sol。")

can_run = bool(packet.get("evidence")) and bool(question.strip())
if st.button("开始 AI + Critic 双重分析", type="primary", use_container_width=True, disabled=not can_run):
    try:
        with st.spinner("正在进行 Analyst 分析并由 Evidence Critic 独立复核…"):
            result = run_audited_ai_analysis(question, packet, api_key=api_key or None, model=model)
            result_dict = result.to_dict()
            st.session_state["audited_ai_result"] = result_dict
    except Exception as exc:
        st.error(f"AI 分析失败：{exc}")

result_dict = st.session_state.get("audited_ai_result")
if result_dict:
    status = result_dict.get("final_status")
    if status == "approved":
        st.success("Evidence Critic：通过，可作为当前证据下的辅助决策建议。")
    elif status == "blocked":
        st.error("Evidence Critic：已阻止。当前证据不允许给出强决策结论。")
    else:
        st.warning("Evidence Critic：需要复核或修改后再用于决策。")

    analyst = result_dict.get("analyst") or {}
    critic = result_dict.get("critic") or {}
    tab1, tab2, tab3 = st.tabs(["AI Analyst", "Evidence Critic", "审计导出"])

    with tab1:
        st.subheader("回答")
        st.write(analyst.get("answer", ""))
        st.subheader("建议")
        st.write(analyst.get("recommendation", ""))
        st.write(f"**置信度：** {analyst.get('confidence', 'unknown')}")
        st.markdown("### 有证据支持的陈述")
        for claim in analyst.get("supported_claims", []):
            ids = ", ".join(claim.get("evidence_ids") or [])
            st.write(f"- {claim.get('claim', '')}  `[{ids}]`")
        if analyst.get("limitations"):
            st.markdown("### 局限")
            for item in analyst["limitations"]:
                st.write(f"- {item}")
        if analyst.get("next_actions"):
            st.markdown("### 下一步")
            for item in analyst["next_actions"]:
                st.write(f"- {item}")

    with tab2:
        st.write(f"**审查结论：** {critic.get('verdict', 'unknown')}")
        st.write(f"**是否允许形成决策建议：** {'是' if critic.get('should_allow_decision') else '否'}")
        st.write(critic.get("corrected_summary", ""))
        if critic.get("issues"):
            st.markdown("### 发现的问题")
            for issue in critic["issues"]:
                ids = ", ".join(issue.get("evidence_ids") or [])
                st.write(f"- **{issue.get('severity')}**：{issue.get('issue')} `[{ids}]`")
        if critic.get("missing_evidence"):
            st.markdown("### 缺失证据")
            for item in critic["missing_evidence"]:
                st.write(f"- {item}")

    with tab3:
        export = {
            "question": question,
            "final_status": status,
            "result": result_dict,
            "evidence_packet": packet,
        }
        st.download_button(
            "下载 AI 审计结果 (.json)",
            data=json.dumps(export, ensure_ascii=False, indent=2),
            file_name="催化剂_AI_审计结果.json",
            mime="application/json",
            use_container_width=True,
        )
        st.caption("导出文件包含 AI 引用的 Evidence IDs 和 Critic 审查结果，但不包含 API Key。")

st.divider()
st.caption("AI 不是证据源。它只能组合、解释和质疑已经进入 Evidence Packet 的信息。")
