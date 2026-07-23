"""Pure data model and statistics for repeatable planner A/B experiments."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from statistics import median
from typing import Any, Iterable


@dataclass(frozen=True)
class TrialRecord:
    planner: str
    trial: int
    success: bool
    arrival_time_s: float
    path_length_m: float
    outcome: int

    def as_dict(self) -> dict[str, Any]:
        return asdict(self)


def summarize(records: Iterable[TrialRecord]) -> dict[str, dict[str, float | int | None]]:
    """Summarize by planner; failed trials never contribute to speed statistics."""
    grouped: dict[str, list[TrialRecord]] = {}
    for record in records:
        grouped.setdefault(record.planner, []).append(record)
    result: dict[str, dict[str, float | int | None]] = {}
    for planner, trials in grouped.items():
        successes = [trial for trial in trials if trial.success]
        result[planner] = {
            "trials": len(trials),
            "successes": len(successes),
            "success_rate": len(successes) / len(trials),
            "median_arrival_time_s": median(t.arrival_time_s for t in successes) if successes else None,
            "median_path_length_m": median(t.path_length_m for t in successes) if successes else None,
        }
    return result
