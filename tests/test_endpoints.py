from src.catlongevity.endpoints import threshold_observation, standard_thresholds


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


def test_exact_threshold():
    obs = threshold_observation([0, 10, 20], [1.0, 0.95, 0.90], 0.90)
    assert obs.status == "exact"
    assert obs.lower_h == 20
    assert obs.upper_h == 20


def test_standard_thresholds_mixed_statuses():
    result = standard_thresholds([0, 20], [1.0, 0.92])
    assert result["t95"].status == "interval"
    assert result["t90"].status == "right_censored"
    assert result["t80"].status == "right_censored"
