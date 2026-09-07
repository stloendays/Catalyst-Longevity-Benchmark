import csv

import pytest

from src.catlongevity.analysis import analyze_observations
from src.catlongevity.io import TOSObservation, group_trajectories, read_tos_csv


def test_group_trajectories_rejects_duplicate_times():
    rows = [
        TOSObservation("A", 0, 20),
        TOSObservation("A", 0, 19),
    ]
    with pytest.raises(ValueError, match="duplicate or non-increasing"):
        group_trajectories(rows)


def test_uncertainty_interval_must_contain_performance():
    with pytest.raises(ValueError, match="inside its uncertainty interval"):
        TOSObservation("A", 0, 20, lower=21, upper=22)


def test_read_tos_csv_preserves_provenance(tmp_path):
    path = tmp_path / "input.csv"
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["paper_id", "catalyst_id", "metric", "time_h", "performance", "provenance_class"])
        writer.writerow(["P_TEST", "A", "CH4_conversion", 0, 20.0, "source_observed"])
    rows = read_tos_csv(path)
    assert rows[0].paper_id == "P_TEST"
    assert rows[0].metric == "CH4_conversion"
    assert rows[0].provenance_class == "source_observed"


def test_end_to_end_report_separates_primary_and_sensitivity_semantics():
    rows = [
        TOSObservation("A", 0, 20.4, paper_id="P_TEST", metric="CH4_conversion", provenance_class="source_observed"),
        TOSObservation("A", 20, 12.6, paper_id="P_TEST", metric="CH4_conversion", provenance_class="source_observed"),
        TOSObservation("B", 0, 17.2, paper_id="P_TEST", metric="CH4_conversion", provenance_class="source_observed"),
        TOSObservation("B", 20, 15.8, paper_id="P_TEST", metric="CH4_conversion", provenance_class="source_observed"),
    ]
    report = analyze_observations(rows)

    assert report["catalysts"]["A"]["threshold_lifetimes"]["t90"]["status"] == "interval"
    assert report["catalysts"]["A"]["cumulative_auc_semantics"] == "derived_piecewise_linear_sensitivity"
    assert report["pairwise"][0]["instantaneous_crossover"]["status"] == "interval"
    assert report["pairwise"][0]["instantaneous_crossover"]["lower_h"] == 0
    assert report["pairwise"][0]["instantaneous_crossover"]["upper_h"] == 20


def test_uncertainty_aware_report_requires_proven_nonoverlap():
    rows = [
        TOSObservation("A", 0, 20, lower=19, upper=21),
        TOSObservation("A", 5, 15, lower=14, upper=16),
        TOSObservation("B", 0, 17, lower=16, upper=18),
        TOSObservation("B", 5, 16, lower=15, upper=17),
    ]
    report = analyze_observations(rows)
    assert report["pairwise"][0]["uncertainty_aware_crossover"]["status"] == "not_proven"
