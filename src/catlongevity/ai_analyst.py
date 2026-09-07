"""Evidence-grounded AI Analyst and Evidence Critic.

The model never receives permission to invent experimental values. It is given a
compact evidence packet produced by deterministic tools and must cite evidence
IDs in every supported claim. A second critic pass can block a recommendation
when comparability, provenance, uncertainty, or evidence sufficiency is weak.
"""
from __future__ import annotations

import json
import os
from dataclasses import dataclass
from typing import Any
from urllib.request import Request, urlopen


OPENAI_RESPONSES_URL = "https://api.openai.com/v1/responses"
DEFAULT_MODEL = "gpt-5.6-terra"


ANALYST_SCHEMA: dict[str, Any] = {
    "type": "object",
    "additionalProperties": False,
    "properties": {
        "answer": {"type": "string"},
        "recommendation": {"type": "string"},
        "confidence": {"type": "string", "enum": ["low", "medium", "high"]},
        "supported_claims": {
            "type": "array",
            "items": {
                "type": "object",
                "additionalProperties": False,
                "properties": {
                    "claim": {"type": "string"},
                    "evidence_ids": {"type": "array", "items": {"type": "string"}},
                },
                "required": ["claim", "evidence_ids"],
            },
        },
        "limitations": {"type": "array", "items": {"type": "string"}},
        "next_actions": {"type": "array", "items": {"type": "string"}},
    },
    "required": ["answer", "recommendation", "confidence", "supported_claims", "limitations", "next_actions"],
}


CRITIC_SCHEMA: dict[str, Any] = {
    "type": "object",
    "additionalProperties": False,
    "properties": {
        "verdict": {"type": "string", "enum": ["pass", "revise", "block"]},
        "should_allow_decision": {"type": "boolean"},
        "issues": {
            "type": "array",
            "items": {
                "type": "object",
                "additionalProperties": False,
                "properties": {
                    "severity": {"type": "string", "enum": ["info", "warning", "critical"]},
                    "issue": {"type": "string"},
                    "evidence_ids": {"type": "array", "items": {"type": "string"}},
                },
                "required": ["severity", "issue", "evidence_ids"],
            },
        },
        "corrected_summary": {"type": "string"},
        "missing_evidence": {"type": "array", "items": {"type": "string"}},
    },
    "required": ["verdict", "should_allow_decision", "issues", "corrected_summary", "missing_evidence"],
}


@dataclass(frozen=True)
class AIAuditResult:
    analyst: dict[str, Any]
    critic: dict[str, Any]
    final_status: str

    def to_dict(self) -> dict[str, Any]:
        return {
            "analyst": self.analyst,
            "critic": self.critic,
            "final_status": self.final_status,
        }


def build_evidence_packet(
    *,
    report: dict[str, Any] | None = None,
    summary: dict[str, Any] | None = None,
    recommendations: list[dict[str, Any]] | None = None,
    evidence_graph: dict[str, Any] | None = None,
    external_records: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    """Build a compact packet with stable evidence IDs for AI review."""
    packet: dict[str, Any] = {
        "rules": {
            "no_invented_values": True,
            "condition_mismatch_blocks_direct_ranking": True,
            "sparse_crossing_is_interval_not_exact_time": True,
            "source_observed_digitized_model_derived_must_remain_distinct": True,
            "external_database_records_are_context_not_longevity_ground_truth": True,
        },
        "evidence": [],
    }
    evidence = packet["evidence"]

    if report:
        condition_audit = report.get("condition_audit") or {}
        evidence.append({"id": "analysis:condition_audit", "type": "condition_audit", "data": condition_audit})
        for catalyst_id, item in (report.get("catalysts") or {}).items():
            evidence.append(
                {
                    "id": f"analysis:catalyst:{catalyst_id}",
                    "type": "trajectory_analysis",
                    "data": item,
                }
            )
        for index, pair in enumerate(report.get("pairwise") or []):
            evidence.append({"id": f"analysis:pair:{index}", "type": "pairwise_analysis", "data": pair})

    if summary:
        evidence.append({"id": "analysis:user_summary", "type": "user_summary", "data": summary})

    if recommendations:
        evidence.append({"id": "analysis:rule_recommendations", "type": "deterministic_recommendations", "data": recommendations})

    if evidence_graph:
        for node in evidence_graph.get("nodes", []) or []:
            node_id = str(node.get("node_id") or "unknown")
            evidence.append({"id": f"graph:{node_id}", "type": "evidence_graph_node", "data": node})

    for index, record in enumerate(external_records or []):
        evidence.append(
            {
                "id": f"external:{index}",
                "type": "external_context",
                "data": record,
            }
        )
    return packet


def deterministic_precheck(packet: dict[str, Any]) -> list[dict[str, str]]:
    """Find hard-stop issues before any LLM call."""
    issues: list[dict[str, str]] = []
    for item in packet.get("evidence", []):
        if item.get("id") == "analysis:condition_audit":
            data = item.get("data") or {}
            if data.get("status") == "mismatch_detected":
                issues.append(
                    {
                        "severity": "critical",
                        "issue": "存在明确实验条件不匹配；相关催化剂对不得直接进行优劣排序。",
                        "evidence_id": "analysis:condition_audit",
                    }
                )
    return issues


def _extract_response_text(payload: dict[str, Any]) -> str:
    for output in payload.get("output", []) or []:
        for content in output.get("content", []) or []:
            if content.get("type") == "output_text" and content.get("text"):
                return str(content["text"])
    raise RuntimeError("AI API 未返回可解析的文本结果")


def _call_openai_structured(
    *,
    instructions: str,
    input_text: str,
    schema_name: str,
    schema: dict[str, Any],
    api_key: str | None = None,
    model: str | None = None,
    timeout: float = 90.0,
) -> dict[str, Any]:
    key = (api_key or os.getenv("OPENAI_API_KEY") or "").strip()
    if not key:
        raise ValueError("AI 分析需要 OpenAI API Key；请在界面临时输入或设置 OPENAI_API_KEY 环境变量。")
    selected_model = (model or os.getenv("OPENAI_MODEL") or DEFAULT_MODEL).strip()
    body = {
        "model": selected_model,
        "instructions": instructions,
        "input": input_text,
        "text": {
            "format": {
                "type": "json_schema",
                "name": schema_name,
                "schema": schema,
                "strict": True,
            }
        },
    }
    request = Request(
        OPENAI_RESPONSES_URL,
        data=json.dumps(body, ensure_ascii=False).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {key}",
            "Content-Type": "application/json",
            "Accept": "application/json",
        },
        method="POST",
    )
    with urlopen(request, timeout=timeout) as response:
        payload = json.loads(response.read().decode("utf-8"))
    return json.loads(_extract_response_text(payload))


def run_ai_analyst(
    question: str,
    packet: dict[str, Any],
    *,
    api_key: str | None = None,
    model: str | None = None,
) -> dict[str, Any]:
    """Generate a Chinese evidence-grounded recommendation."""
    if not question.strip():
        raise ValueError("请输入希望 AI 分析的问题")
    precheck = deterministic_precheck(packet)
    prompt = json.dumps({"question": question, "packet": packet, "deterministic_precheck": precheck}, ensure_ascii=False)
    instructions = (
        "你是催化剂长期表现分析员。只能依据输入 packet 中的证据回答，禁止补造实验值、文献结论或精确交叉时间。"
        "所有 supported_claims 必须引用 packet 中真实存在的 evidence id。外部数据库只可作为背景，不可替代 TOS 稳定性证据。"
        "若实验条件不匹配、证据不足或结果仅为候选值，必须明确降低 confidence 并限制 recommendation。"
        "不要输出思维过程，只输出结构化结论。回答使用简体中文。"
    )
    return _call_openai_structured(
        instructions=instructions,
        input_text=prompt,
        schema_name="catalyst_ai_analyst",
        schema=ANALYST_SCHEMA,
        api_key=api_key,
        model=model,
    )


def run_evidence_critic(
    question: str,
    packet: dict[str, Any],
    analyst_result: dict[str, Any],
    *,
    api_key: str | None = None,
    model: str | None = None,
) -> dict[str, Any]:
    """Independently audit the analyst result and block unsupported decisions."""
    precheck = deterministic_precheck(packet)
    prompt = json.dumps(
        {
            "question": question,
            "packet": packet,
            "analyst_result": analyst_result,
            "deterministic_precheck": precheck,
        },
        ensure_ascii=False,
    )
    instructions = (
        "你是独立 Evidence Critic，不负责迎合 Analyst。检查：实验条件是否可比、引用的 evidence id 是否存在、"
        "source-observed/digitized/model-derived 是否被混淆、稀疏点是否被宣称为精确交叉时间、删失寿命是否被当成精确寿命、"
        "外部数据库背景是否被错误当成长期性能证据。存在硬性条件不匹配或关键证据缺失时 verdict 必须为 block。"
        "可以要求 revise。不要输出思维过程，只输出结构化审查结论。回答使用简体中文。"
    )
    return _call_openai_structured(
        instructions=instructions,
        input_text=prompt,
        schema_name="catalyst_evidence_critic",
        schema=CRITIC_SCHEMA,
        api_key=api_key,
        model=model,
    )


def run_audited_ai_analysis(
    question: str,
    packet: dict[str, Any],
    *,
    api_key: str | None = None,
    model: str | None = None,
) -> AIAuditResult:
    analyst = run_ai_analyst(question, packet, api_key=api_key, model=model)
    critic = run_evidence_critic(question, packet, analyst, api_key=api_key, model=model)
    verdict = critic.get("verdict", "block")
    final_status = "approved" if verdict == "pass" and critic.get("should_allow_decision") else "needs_review"
    if verdict == "block":
        final_status = "blocked"
    return AIAuditResult(analyst=analyst, critic=critic, final_status=final_status)
