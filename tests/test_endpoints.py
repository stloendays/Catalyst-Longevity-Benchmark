from src.catlongevity.endpoints import (
    exact_threshold_observation,
    standard_thresholds,
    threshold_observation,
)


def test_right_censored_threshold():
    obs = threshold_observation([0, 10, 20], [1.0, 0.97, 0.94], 0.90)
    assert obs.status == "right_censored"
    assert obs.lower_h == 20
    assert obs.upper_h is None


def test_interval_censored_threshold():
    obs = threshold_observation([0, 10, 20], [1.0, 0.93, 0.88], 0.90)
    assert obs.status == "interval"
    assert obs.lower_h == 10
    assert obs.upper_h == 20


def test_sample_equal_to_threshold_is_still_interval():
    obs = threshold_observation([0, 10, 20], [1.0, 0.95, 0.90], 0.90)
    assert obs.status == "interval"
    assert obs.lower_h == 10
    assert obs.upper_h == 20


def test_first_sample_at_threshold_is_left_censored():
    obs = threshold_observation([5, 10], [0.90, 0.88], 0.90)
    assert obs.status == "left_censored"
    assert obs.lower_h == 0
    assert obs.upper_h == 5


def test_exact_requires_direct_source_report():
    obs = exact_threshold_observation(0.90, 17.5)
    assert obs.status == "exact"
    assert obs.lower_h == 17.5
    assert obs.upper_h == 17.5


def test_standard_thresholds_mixed_statuses():
    result = standard_thresholds([0, 20], [1.0, 0.92])
    assert result["t95"].status == "interval"
    assert result["t90"].status == "right_censored"
    assert result["t80"].status == "right_censored"
