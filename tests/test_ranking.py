from src.catlongevity.ranking import (
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
