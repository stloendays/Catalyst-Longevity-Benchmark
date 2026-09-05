from src.catlongevity.ranking import (
    cumulative_trapezoid_at_observed_times,
    descending_rank,
    first_pairwise_crossover_bracket,
    linear_crossover_estimate,
)


def test_observed_rank_reversal_bracket():
    obs = first_pairwise_crossover_bracket(
        [0, 20],
        [20.4, 12.6],
        [17.2, 15.8],
    )
    assert obs.status == "interval"
    assert obs.lower_h == 0
    assert obs.upper_h == 20
    assert obs.direction == "A_to_B"


def test_linear_crossover_estimate_zhang_550c():
    t_cross = linear_crossover_estimate(
        0, 20,
        20.4, 12.6,
        17.2, 15.8,
    )
    assert abs(t_cross - 10.0) < 1e-12


def test_no_observed_reversal():
    obs = first_pairwise_crossover_bracket(
        [0, 20],
        [26.9, 19.1],
        [19.4, 18.6],
    )
    assert obs.status == "not_observed"
    assert obs.lower_h == 20
    assert obs.upper_h is None


def test_zhou2015_full_instantaneous_rank_reversal():
    t = [0.5, 15, 40, 100]
    y350 = [6.5, 3.2, 2.6, 1.7]
    y700 = [5.9, 3.6, 3.2, 2.9]
    y900 = [5.1, 4.5, 4.4, 4.4]

    assert descending_rank({"350": y350[0], "700": y700[0], "900": y900[0]}) == ["350", "700", "900"]
    assert descending_rank({"350": y350[1], "700": y700[1], "900": y900[1]}) == ["900", "700", "350"]

    for a, b in ((y350, y700), (y350, y900), (y700, y900)):
        obs = first_pairwise_crossover_bracket(t, a, b)
        assert obs.status == "interval"
        assert obs.lower_h == 0.5
        assert obs.upper_h == 15


def test_zhou2015_cumulative_rank_lags_instantaneous_rank():
    t = [0.5, 15, 40, 100]
    curves = {
        "350": [6.5, 3.2, 2.6, 1.7],
        "700": [5.9, 3.6, 3.2, 2.9],
        "900": [5.1, 4.5, 4.4, 4.4],
    }
    auc = {name: cumulative_trapezoid_at_observed_times(t, y) for name, y in curves.items()}

    assert auc["350"] == [0.0, 70.325, 142.825, 271.825]
    assert auc["700"] == [0.0, 68.875, 153.875, 336.875]
    assert auc["900"] == [0.0, 69.6, 180.85, 444.85]

    # At 15 h, instantaneous ranking has already reversed to 900 > 700 > 350,
    # but cumulative production still carries an early-activity advantage.
    assert descending_rank({name: vals[1] for name, vals in auc.items()}) == ["350", "900", "700"]
    assert descending_rank({name: vals[2] for name, vals in auc.items()}) == ["900", "700", "350"]
    assert descending_rank({name: vals[3] for name, vals in auc.items()}) == ["900", "700", "350"]
