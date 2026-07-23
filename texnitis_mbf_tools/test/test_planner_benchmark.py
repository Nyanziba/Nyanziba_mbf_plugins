from texnitis_mbf_tools.planner_benchmark import TrialRecord, summarize


def test_summarize_reports_success_rate_and_median_arrival_time():
    records = [
        TrialRecord("astar", 1, True, 10.0, 5.0, 0),
        TrialRecord("astar", 2, False, 12.0, 5.2, 1),
        TrialRecord("kinematic", 1, True, 8.0, 5.1, 0),
        TrialRecord("kinematic", 2, True, 9.0, 5.0, 0),
    ]

    result = summarize(records)

    assert result["astar"]["success_rate"] == 0.5
    assert result["astar"]["median_arrival_time_s"] == 10.0
    assert result["kinematic"]["median_arrival_time_s"] == 8.5


def test_summarize_does_not_treat_failed_trial_as_fast_arrival():
    records = [TrialRecord("astar", 1, False, 0.1, 0.0, 50)]

    result = summarize(records)

    assert result["astar"]["median_arrival_time_s"] is None


def test_summarize_empty_input_is_empty():
    assert summarize([]) == {}
