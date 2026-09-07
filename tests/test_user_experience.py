from src.catlongevity.analysis import analyze_observations
from src.catlongevity.friendly import build_user_summary, describe_threshold
from src.catlongevity.io import observations_from_records


def test_chinese_column_aliases_work():
    rows = [
        {"催化剂": "A", "时间": 0, "性能": 100},
        {"催化剂": "A", "时间": 10, "性能": 88},
        {"催化剂": "B", "时间": 0, "性能": 90},
        {"催化剂": "B", "时间": 10, "性能": 89},
    ]
    observations = observations_from_records(rows)
    assert len(observations) == 4
    assert observations[0].catalyst_id == "A"
    assert observations[1].time_h == 10


def test_user_summary_reports_rank_change():
    rows = [
        {"catalyst": "A", "time": 0, "value": 100},
        {"catalyst": "A", "time": 10, "value": 70},
        {"catalyst": "B", "time": 0, "value": 90},
        {"catalyst": "B", "time": 10, "value": 80},
    ]
    report = analyze_observations(observations_from_records(rows))
    summary = build_user_summary(report)
    assert summary["catalyst_count"] == 2
    assert summary["observed_rank_changes"] == 1
    assert "反超" in summary["pairwise_conclusions"][0]


def test_right_censored_threshold_has_plain_language():
    text = describe_threshold(
        "t90",
        {"threshold": 0.90, "status": "right_censored", "lower_h": 100, "upper_h": None},
    )
    assert "100" in text
    assert "仍高于" in text
