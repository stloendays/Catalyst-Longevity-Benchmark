"""Small auditable evidence graph used by document and database workflows."""
from __future__ import annotations

from dataclasses import asdict, dataclass, field
from typing import Any


@dataclass
class EvidenceNode:
    node_id: str
    kind: str
    label: str
    source: str
    value: Any = None
    confidence: str = "source_detected"
    locator: str | None = None
    notes: str | None = None


@dataclass
class EvidenceEdge:
    source_id: str
    relation: str
    target_id: str


@dataclass
class EvidenceGraph:
    nodes: list[EvidenceNode] = field(default_factory=list)
    edges: list[EvidenceEdge] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        return {
            "nodes": [asdict(node) for node in self.nodes],
            "edges": [asdict(edge) for edge in self.edges],
        }


def build_document_evidence_graph(filename: str, signals: dict[str, Any], crossref: dict[str, Any] | None = None) -> EvidenceGraph:
    graph = EvidenceGraph()
    document_id = "document:0"
    graph.nodes.append(EvidenceNode(document_id, "document", filename, source="用户上传资料", confidence="user_supplied"))

    for index, doi in enumerate(signals.get("dois", [])):
        node_id = f"doi:{index}"
        graph.nodes.append(EvidenceNode(node_id, "identifier", "DOI", source=filename, value=doi))
        graph.edges.append(EvidenceEdge(document_id, "contains", node_id))

    for index, temp in enumerate(signals.get("temperatures_c", [])):
        node_id = f"temperature:{index}"
        graph.nodes.append(EvidenceNode(node_id, "condition", "温度", source=filename, value=f"{temp:g} °C"))
        graph.edges.append(EvidenceEdge(document_id, "mentions", node_id))

    for index, duration in enumerate(signals.get("durations_h", [])):
        node_id = f"duration:{index}"
        graph.nodes.append(EvidenceNode(node_id, "condition", "测试时长候选", source=filename, value=f"{duration:g} h"))
        graph.edges.append(EvidenceEdge(document_id, "mentions", node_id))

    for index, value in enumerate(signals.get("ch4_conversion_percent_candidates", [])):
        node_id = f"ch4_conversion:{index}"
        graph.nodes.append(
            EvidenceNode(
                node_id,
                "measurement_candidate",
                "CH4 转化率候选值",
                source=filename,
                value=f"{value:g}%",
                confidence="candidate_requires_condition_binding",
                notes="进入排名前必须绑定到具体催化剂、时间和实验条件。",
            )
        )
        graph.edges.append(EvidenceEdge(document_id, "contains_candidate", node_id))

    if crossref:
        metadata_id = "external:crossref"
        graph.nodes.append(
            EvidenceNode(
                metadata_id,
                "external_metadata",
                crossref.get("title") or "Crossref metadata",
                source="Crossref",
                value={
                    "doi": crossref.get("doi"),
                    "journal": crossref.get("journal"),
                    "year": crossref.get("year"),
                    "authors": crossref.get("authors"),
                },
                confidence="external_metadata",
            )
        )
        graph.edges.append(EvidenceEdge(document_id, "identity_checked_by", metadata_id))
    return graph
