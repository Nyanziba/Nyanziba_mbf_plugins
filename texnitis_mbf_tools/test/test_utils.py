"""Unit tests for texnitis_mbf_tools.utils.

ROS が source されていなくても走るよう、`rclpy` には触らない。
"""

from __future__ import annotations

import math
from pathlib import Path

import pytest

from texnitis_mbf_tools.utils import (
    load_waypoints_file,
    parse_waypoints,
    yaw_from_quaternion,
    yaw_to_quaternion,
)


def test_yaw_to_quaternion_zero() -> None:
    """yaw=0 で (0,0,0,1)."""
    assert yaw_to_quaternion(0.0) == pytest.approx((0.0, 0.0, 0.0, 1.0))


def test_yaw_to_quaternion_round_trip() -> None:
    """yaw -> quat -> yaw が一致する."""
    for yaw in [0.0, 0.5, -0.5, 1.5, -1.5, math.pi - 0.01, -math.pi + 0.01]:
        qx, qy, qz, qw = yaw_to_quaternion(yaw)
        recovered = yaw_from_quaternion(qx, qy, qz, qw)
        assert recovered == pytest.approx(yaw, abs=1e-9)


def test_yaw_from_identity_quaternion() -> None:
    """Identity quaternion -> yaw=0."""
    assert yaw_from_quaternion(0.0, 0.0, 0.0, 1.0) == pytest.approx(0.0)


def test_parse_waypoints_minimal() -> None:
    """frame_id, loop, waypoints の 3 つだけのケース."""
    spec = {
        "frame_id": "map",
        "loop": False,
        "waypoints": [{"x": 1.0, "y": 0.0, "yaw": 0.0}],
    }
    waypoints, frame_id, loop = parse_waypoints(spec)
    assert frame_id == "map"
    assert loop is False
    assert waypoints == [{"x": 1.0, "y": 0.0, "yaw": 0.0}]


def test_parse_waypoints_default_yaw() -> None:
    """yaw が無い waypoint は 0.0 が補完される."""
    spec = {"waypoints": [{"x": 0.5, "y": 0.5}]}
    waypoints, _, _ = parse_waypoints(spec)
    assert waypoints[0]["yaw"] == 0.0


def test_parse_waypoints_default_frame() -> None:
    """frame_id が無いと map になる."""
    spec = {"waypoints": [{"x": 0.5, "y": 0.5}]}
    _, frame_id, _ = parse_waypoints(spec)
    assert frame_id == "map"


def test_parse_waypoints_default_loop() -> None:
    """loop が無いと False になる."""
    spec = {"waypoints": [{"x": 0.5, "y": 0.5}]}
    _, _, loop = parse_waypoints(spec)
    assert loop is False


def test_parse_waypoints_rejects_empty() -> None:
    """空の waypoints は ValueError."""
    with pytest.raises(ValueError, match="must not be empty"):
        parse_waypoints({"waypoints": []})


def test_parse_waypoints_rejects_missing_xy() -> None:
    """x / y が欠けたら ValueError."""
    with pytest.raises(ValueError, match="missing or invalid"):
        parse_waypoints({"waypoints": [{"y": 1.0}]})


def test_parse_waypoints_rejects_non_string_frame() -> None:
    """frame_id が文字列でないと ValueError."""
    with pytest.raises(ValueError, match="frame_id"):
        parse_waypoints({"frame_id": 42, "waypoints": [{"x": 0.0, "y": 0.0}]})


def test_load_waypoints_file(tmp_path: Path) -> None:
    """ファイルから読み込めることを確認."""
    yaml_path = tmp_path / "wp.yaml"
    yaml_path.write_text(
        "frame_id: my_frame\n"
        "loop: true\n"
        "waypoints:\n"
        "  - {x: 1.0, y: 2.0, yaw: 3.14}\n"
    )
    waypoints, frame_id, loop = load_waypoints_file(yaml_path)
    assert frame_id == "my_frame"
    assert loop is True
    assert waypoints == [{"x": 1.0, "y": 2.0, "yaw": 3.14}]
