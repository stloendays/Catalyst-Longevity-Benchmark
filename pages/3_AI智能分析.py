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
from src.catlongevity.ui import (
    apply_global_styles,
    empty_state,
    render_hero,
    render_sidebar,
    render_workflow,
    section_title,
    status_box,
)


st.set_page_config(page_title="AI 智能分析｜催化剂智能分析平台", page_icon="🧠", layout="wide", initial_sidebar_state="expanded")
apply_global_styles()
render_sidebar("ai")
render_hero(
    "AI 智能分析",
    "让 AI Analyst 综合结构化实验结果、资料证据和外部背景，再由独立 Evidence Critic 检查证据引用、条件错配和过度推断。",
    eyebrow="STEP 04 · Audited AI Decision",
)
render_workflow("ai")

report = st.session_state.get("analysis_report")
summary = st.session_state.get("analysis_summary")
recommendations = st.session_state.get("analysis_recommendations")
document_graph = st.session_state.get("document_evidence_graph")
external_records = st.session_state.get("external_records", [])

section_title("证据准备度", "AI 不会把自己当作证据源")
c1, c2, c3, c4 = st.columns(4)
c1.metric("长期表现", "已加载" if report else "未加载")
c2.metric("资料图谱", "已加载" if document_graph else "未加载")
c3.metric("外部记录", len(external_records))
c4.metric("上次审查", "已有" if st.session_state.get("audited_ai_result") else "未运行")

if not report and not document_graph and not external_records:
    empty_state("当前没有可供 AI 使用的证据", "先完成长期表现分析、上传资料，或从外部数据库加入证据篮。")
else:
    if not report:
        status_box(
            "尚未加载长期表现数据",
            "AI 可以帮助梳理资料证据和缺口，但不会在没有结构化 TOS 数据时凭空生成催化剂长期排名。",
            tone="warning",
        )

packet = build_evidence_packet(
    report=report,
    summary=summary,
    recommendations=recommendations,
    evidence_graph=document_graph,
    external_records=external_records,
)

precheck = deterministic_precheck(packet)
if precheck:
    status_box(
        "确定性规则发现硬性问题",
        "AI 可以解释这些问题，但不能绕过条件守门、证据分层或删失寿命规则。",
        tone="danger",
    )
    with st.expander("查看阻断原因", expanded=True):
        for issue in precheck:
            st.write(f"- {issue['issue']}  `{issue['evidence_id']}`")
else:
    if packet.get("evidence"):
        status_box("前置审计通过", "未发现需要在 AI 调用前直接阻断的确定性问题。", tone="success")

with st.expander("查看 Evidence Packet", expanded=False):
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
        empty_state("Evidence Packet 为空", "没有证据时 AI 分析按钮会保持禁用。")

section_title("提出问题", "建议明确目标运行时间、比较对象和你希望得到的决策")
question = st.text_area(
    "问题",
    value="结合当前实验数据、资料和外部背景，哪个催化剂更适合更长时间运行？现有证据能支持到什么程度，还需要补什么数据？",
    height=130,
    label_visibility="collapsed",
)

with st.expander("问题示例"):
    st.write("- 如果目标运行 100 h，A 和 B 哪个更值得优先验证？")
    st.write("- 目前证据是否足以证明发生了长期排名反转？")
    st.write("- 下一次实验最值得增加哪个时间点，为什么？")
    st.write("- 外部材料数据库信息能否支持当前的失活机制解释？")

settings_col, action_col = st.columns([1, 1.7], gap="large")
with settings_col:
    with st.container(border=True):
        st.markdown("#### AI 设置")
        model = st.selectbox("模型", ["gpt-5.6-terra", "gpt-5.6-sol", "gpt-5.6-luna"], index=0)
        api_key = st.text_input(
            "OpenAI API Key",
            type="password",
            help="只用于当前调用；不写入仓库、Evidence Packet 或导出结果。也可通过 OPENAI_API_KEY 环境变量提供。",
        )
        st.caption("复杂证据审查可选择更强模型；密钥不会进入导出文件。")

with action_col:
    with st.container(border=True):
        st.markdown("#### 审计流程")
        st.write("**1. Analyst**：只基于 Evidence Packet 形成回答与建议")
        st.write("**2. Critic**：独立检查证据引用、条件错配、过度外推与遗漏")
        st.write("**3. Final status**：`approved / needs_review / blocked`")
        st.caption("Evidence Critic 可以阻止 Analyst 的结论进入决策状态。")

can_run = bool(packet.get("evidence")) and bool(question.strip())
run = st.button(
    "开始 Analyst + Critic 双重分析",
    type="primary",
    use_container_width=True,
    disabled=not can_run,
)
if run:
    try:
        with st.spinner("Analyst 正在综合证据，Evidence Critic 随后独立复核…"):
            result = run_audited_ai_analysis(question, packet, api_key=api_key or None, model=model)
            result_dict = result.to_dict()
            st.session_state["audited_ai_result"] = result_dict
    except Exception as exc:
        status_box("AI 分析失败", str(exc), tone="danger")

result_dict = st.session_state.get("audited_ai_result")
if result_dict:
    section_title("审查结果", "先看 Critic 状态，再阅读 Analyst 内容")
    status = result_dict.get("final_status")
    if status == "approved":
        status_box("Evidence Critic：通过", "当前结果可作为现有证据条件下的辅助决策建议。", tone="success")
    elif status == "blocked":
        status_box("Evidence Critic：已阻止", "当前证据不允许形成强决策结论，请先处理下方指出的问题。", tone="danger")
    else:
        status_box("Evidence Critic：需要复核", "结论有一定依据，但应修改、补证据或降低表述强度后再用于决策。", tone="warning")

    analyst = result_dict.get("analyst") or {}
    critic = result_dict.get("critic") or {}
    tab1, tab2, tab3 = st.tabs(["最终回答", "Critic 审计", "Evidence IDs 与导出"])

    with tab1:
        answer_col, meta_col = st.columns([2, 1], gap="large")
        with answer_col:
            section_title("AI Analyst 回答")
            with st.container(border=True):
                st.write(analyst.get("answer", ""))
            section_title("建议")
            with st.container(border=True):
                st.write(analyst.get("recommendation", ""))
        with meta_col:
            section_title("回答状态")
            st.metric("Analyst 置信度", analyst.get("confidence", "unknown"))
            st.metric("支持性陈述", len(analyst.get("supported_claims", [])))
            st.metric("局限", len(analyst.get("limitations", [])))

        if analyst.get("limitations"):
            section_title("局限")
            for item in analyst["limitations"]:
                status_box("限制条件", item, tone="warning")
        if analyst.get("next_actions"):
            section_title("下一步")
            for index, item in enumerate(analyst["next_actions"], start=1):
                status_box(f"行动 {index}", item)

    with tab2:
        k1, k2 = st.columns(2)
        k1.metric("Critic verdict", critic.get("verdict", "unknown"))
        k2.metric("允许决策", "是" if critic.get("should_allow_decision") else "否")
        if critic.get("corrected_summary"):
            section_title("Critic 修正摘要")
            with st.container(border=True):
                st.write(critic.get("corrected_summary", ""))
        if critic.get("issues"):
            section_title("发现的问题")
            for issue in critic["issues"]:
                ids = ", ".join(issue.get("evidence_ids") or [])
                severity = issue.get("severity") or "issue"
                tone = "danger" if severity.lower() in {"high", "critical", "blocker"} else "warning"
                status_box(severity, f"{issue.get('issue')} · Evidence: {ids or '未提供'}", tone=tone)
        if critic.get("missing_evidence"):
            section_title("缺失证据")
            for item in critic["missing_evidence"]:
                status_box("需要补充", item, tone="warning")

    with tab3:
        section_title("有证据支持的陈述")
        claims = analyst.get("supported_claims", [])
        if claims:
            claim_rows = []
            for claim in claims:
                claim_rows.append(
                    {
                        "陈述": claim.get("claim", ""),
                        "Evidence IDs": ", ".join(claim.get("evidence_ids") or []),
                    }
                )
            st.dataframe(pd.DataFrame(claim_rows), use_container_width=True, hide_index=True)
        else:
            empty_state("没有结构化支持性陈述", "这通常意味着 AI 没有给出可审计的 claim-to-evidence 映射。")

        export = {
            "question": question,
            "final_status": status,
            "result": result_dict,
            "evidence_packet": packet,
        }
        st.download_button(
            "下载完整 AI 审计结果 · JSON",
            data=json.dumps(export, ensure_ascii=False, indent=2),
            file_name="催化剂_AI_审计结果.json",
            mime="application/json",
            use_container_width=True,
        )
        st.caption("导出包含 Evidence IDs 和 Critic 审查结果，不包含 API Key。")

st.divider()
st.caption("Audited AI Decision · AI 只能组合、解释和质疑 Evidence Packet 中已有的信息")
