"""ROS-free helpers reused by the mbf tools.

These functions are kept independent of rclpy / ROS message types so they
can be unit-tested without sourcing a ROS distribution.
"""

from __future__ import annotations

import math
from pathlib import Path
from typing import Any, Tuple


def yaw_to_quaternion(yaw: float) -> Tuple[float, float, float, float]:
    """Return (x, y, z, w) for a quaternion that represents `yaw` only."""
    return (0.0, 0.0, math.sin(yaw / 2.0), math.cos(yaw / 2.0))


def yaw_from_quaternion(qx: float, qy: float, qz: float, qw: float) -> float:
    """Extract yaw from a (x, y, z, w) quaternion."""
    siny_cosp = 2.0 * (qw * qz + qx * qy)
    cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz)
    return math.atan2(siny_cosp, cosy_cosp)


def parse_waypoints(spec: dict[str, Any]) -> Tuple[list[dict[str, float]], str, bool]:
    """Validate a waypoints YAML doc and return (waypoints, frame_id, loop)."""
    if not isinstance(spec, dict):
        raise ValueError("waypoints YAML must be a mapping")
    if "waypoints" not in spec:
        raise ValueError("missing required key 'waypoints'")

    frame_id = spec.get("frame_id", "map")
    if not isinstance(frame_id, str) or not frame_id:
        raise ValueError("'frame_id' must be a non-empty string")

    loop = bool(spec.get("loop", False))

    waypoints: list[dict[str, float]] = []
    for idx, wp in enumerate(spec["waypoints"]):
        if not isinstance(wp, dict):
            raise ValueError(f"waypoint #{idx} must be a mapping")
        try:
            x = float(wp["x"])
            y = float(wp["y"])
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(f"waypoint #{idx} missing or invalid x/y") from exc
        yaw = float(wp.get("yaw", 0.0))
        waypoints.append({"x": x, "y": y, "yaw": yaw})

    if not waypoints:
        raise ValueError("'waypoints' must not be empty")

    return waypoints, frame_id, loop


def load_waypoints_file(path: Path | str) -> Tuple[list[dict[str, float]], str, bool]:
    """Load `path` (YAML) and return (waypoints, frame_id, loop)."""
    import yaml

    with Path(path).open("r", encoding="utf-8") as handle:
        spec = yaml.safe_load(handle)
    return parse_waypoints(spec)
