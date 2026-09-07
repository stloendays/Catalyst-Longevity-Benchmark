from src.catlongevity.advisor import build_document_advice, build_recommendations
from src.catlongevity.analysis import analyze_observations
from src.catlongevity.condition_matcher import apply_condition_guard, audit_conditions
from src.catlongevity.documents import extract_document_signals
from src.catlongevity.evidence import build_document_evidence_graph
from src.catlongevity.external_databases import normalize_doi
from src.catlongevity.friendly import build_user_summary
from src.catlongevity.io import observations_from_records


def test_normalize_doi_accepts_url():
    assert normalize_doi("https://doi.org/10.1039/C9CY02093D") == "10.1039/C9CY02093D"


def test_document_signal_extraction_is_conservative():
    text = (
        "DOI: 10.1039/C9CY02093D. The catalyst was tested at 700 °C for 100 h. "
        "CH4 conversion was 81.2%. Carbon deposition and coking were investigated."
    )
    signals = extract_document_signals(text)
    assert signals["dois"] == ["10.1039/C9CY02093D"]
    assert 700.0 in signals["temperatures_c"]
    assert 100.0 in signals["durations_h"]
    assert 81.2 in signals["ch4_conversion_percent_candidates"]
    assert "coking" in signals["keyword_evidence"]


def test_document_evidence_graph_keeps_candidates_separate():
    signals = {
        "dois": ["10.1000/example"],
        "temperatures_c": [700.0],
        "durations_h": [50.0],
        "ch4_conversion_percent_candidates": [80.0],
    }
    graph = build_document_evidence_graph("paper.pdf", signals).to_dict()
    candidate = [node for node in graph["nodes"] if node["kind"] == "measurement_candidate"][0]
    assert candidate["confidence"] == "candidate_requires_condition_binding"
    assert "进入排名前" in candidate["notes"]


def test_recommendation_engine_suggests_sampling_inside_crossover_interval():
    rows = [
        {"催化剂": "A", "时间": 0, "性能": 100},
        {"催化剂": "A", "时间": 20, "性能": 70},
        {"催化剂": "B", "时间": 0, "性能": 90},
        {"催化剂": "B", "时间": 20, "性能": 80},
    ]
    report = analyze_observations(observations_from_records(rows))
    summary = build_user_summary(report)
    recommendations = build_recommendations(report, summary)
    assert any("最佳选择随时间改变" in item["title"] for item in recommendations)
    assert any("增加测试点" in item["title"] for item in recommendations)


def test_document_advice_requests_condition_binding_for_conversion_candidates():
    signals = {
        "dois": [],
        "durations_h": [20.0],
        "ch4_conversion_percent_candidates": [80.0],
        "keyword_evidence": {},
    }
    advice = build_document_advice(signals)
    assert any("绑定到具体催化剂" in item for item in advice)


def test_condition_guard_disables_mismatched_temperature_comparison():
    rows = [
        {"催化剂": "A", "时间": 0, "性能": 100, "温度": 700},
        {"催化剂": "A", "时间": 20, "性能": 70, "温度": 700},
        {"催化剂": "B", "时间": 0, "性能": 90, "温度": 600},
        {"催化剂": "B", "时间": 20, "性能": 80, "温度": 600},
    ]
    audit = audit_conditions(rows)
    assert audit["status"] == "mismatch_detected"
    report = analyze_observations(observations_from_records(rows))
    guarded = apply_condition_guard(report, audit)
    assert guarded["pairwise"][0]["instantaneous_crossover"]["status"] == "not_evaluable"
    assert "temperature_c" in guarded["pairwise"][0]["condition_mismatch_fields"]


def test_condition_guard_allows_matched_conditions():
    rows = [
        {"催化剂": "A", "时间": 0, "性能": 100, "温度": 700, "GHSV": 18000},
        {"催化剂": "A", "时间": 20, "性能": 70, "温度": 700, "GHSV": 18000},
        {"催化剂": "B", "时间": 0, "性能": 90, "温度": 700, "GHSV": 18000},
        {"催化剂": "B", "时间": 20, "性能": 80, "温度": 700, "GHSV": 18000},
    ]
    audit = audit_conditions(rows)
    assert audit["status"] == "matched_on_provided_conditions"
    report = analyze_observations(observations_from_records(rows))
    guarded = apply_condition_guard(report, audit)
    assert guarded["pairwise"][0]["instantaneous_crossover"]["status"] == "interval"


def test_condition_normalization_treats_700_and_700_point_zero_as_same():
    rows = [
        {"催化剂": "A", "时间": 0, "性能": 100, "温度": 700, "进料比": "1 : 1"},
        {"催化剂": "A", "时间": 20, "性能": 80, "温度": 700, "进料比": "1 : 1"},
        {"催化剂": "B", "时间": 0, "性能": 90, "温度": "700.0", "进料比": "1:1"},
        {"催化剂": "B", "时间": 20, "性能": 85, "温度": "700.0", "进料比": "1:1"},
    ]
    audit = audit_conditions(rows)
    assert audit["status"] == "matched_on_provided_conditions"
