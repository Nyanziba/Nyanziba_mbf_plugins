#!/usr/bin/env python3
"""Re-emit a legacy-style ``nav_state`` JSON from mbf feedback.

The legacy ``move_base_like_node`` published a JSON String on
``/move_base_like_node/nav_state`` with fields like ``has_goal``,
``has_path``, ``distance_to_goal`` and ``last_error``. Several
downstream consumers (the old WebUI dashboard, the legacy
waypoint sender, etc.) rely on those keys.

This bridge node subscribes to the mbf MoveBase action's feedback /
result and the global path topic, and republishes a compatible JSON on
``/texnitis_mbf/nav_state``. Drop-in for the legacy topic name with a
remap.
"""

from __future__ import annotations

import json
import math
from typing import Any, Optional

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped
from mbf_msgs.action import MoveBase
from nav_msgs.msg import OccupancyGrid, Path
from rclpy.action import ActionClient
from rclpy.node import Node
from std_msgs.msg import String


def yaw_from_quat(qx: float, qy: float, qz: float, qw: float) -> float:
    """Extract yaw from a (x, y, z, w) quaternion."""
    siny_cosp = 2.0 * (qw * qz + qx * qy)
    cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz)
    return math.atan2(siny_cosp, cosy_cosp)


class NavStatePublisher(Node):
    """Aggregates topic state into a legacy-compatible JSON message."""

    def __init__(self) -> None:
        super().__init__("nav_state_publisher")
        self.declare_parameter("publish_topic", "/texnitis_mbf/nav_state")
        self.declare_parameter("plan_topic", "/move_base_flex/global_plan")
        self.declare_parameter("map_topic", "/map")
        self.declare_parameter("robot_pose_topic", "/move_base_flex/robot_pose")
        self.declare_parameter("goal_pose_topic", "/goal_pose")
        self.declare_parameter("publish_hz", 10.0)

        publish_topic = self.get_parameter("publish_topic").value
        plan_topic = self.get_parameter("plan_topic").value
        map_topic = self.get_parameter("map_topic").value
        pose_topic = self.get_parameter("robot_pose_topic").value
        goal_topic = self.get_parameter("goal_pose_topic").value
        publish_hz = float(self.get_parameter("publish_hz").value)

        self._has_map = False
        self._has_path = False
        self._has_goal = False
        self._has_pose = False
        self._path_length = 0
        self._fused: Optional[tuple[float, float, float]] = None
        self._goal: Optional[tuple[float, float, float]] = None
        self._last_error = ""

        self.create_subscription(OccupancyGrid, map_topic, self._on_map, 1)
        self.create_subscription(Path, plan_topic, self._on_path, 1)
        self.create_subscription(PoseStamped, pose_topic, self._on_pose, 10)
        self.create_subscription(PoseStamped, goal_topic, self._on_goal, 1)

        self._publisher = self.create_publisher(String, publish_topic, 10)
        self.create_timer(1.0 / max(publish_hz, 1.0), self._publish_state)

    def _on_map(self, _msg: OccupancyGrid) -> None:
        self._has_map = True

    def _on_path(self, msg: Path) -> None:
        self._path_length = len(msg.poses)
        self._has_path = self._path_length > 0

    def _on_pose(self, msg: PoseStamped) -> None:
        yaw = yaw_from_quat(
            msg.pose.orientation.x,
            msg.pose.orientation.y,
            msg.pose.orientation.z,
            msg.pose.orientation.w,
        )
        self._fused = (msg.pose.position.x, msg.pose.position.y, yaw)
        self._has_pose = True

    def _on_goal(self, msg: PoseStamped) -> None:
        yaw = yaw_from_quat(
            msg.pose.orientation.x,
            msg.pose.orientation.y,
            msg.pose.orientation.z,
            msg.pose.orientation.w,
        )
        self._goal = (msg.pose.position.x, msg.pose.position.y, yaw)
        self._has_goal = True

    def _publish_state(self) -> None:
        state: dict[str, Any] = {
            "has_goal": self._has_goal,
            "has_path": self._has_path,
            "has_map": self._has_map,
            "has_pose": self._has_pose,
            "path_length": self._path_length,
        }
        if self._fused is not None:
            fx, fy, fyaw = self._fused
            state["fused_x"] = fx
            state["fused_y"] = fy
            state["fused_yaw"] = fyaw
        if self._goal is not None:
            gx, gy, gyaw = self._goal
            state["goal_x"] = gx
            state["goal_y"] = gy
            state["goal_yaw"] = gyaw
            if self._fused is not None:
                fx, fy, _ = self._fused
                state["distance_to_goal"] = math.hypot(gx - fx, gy - fy)
        if self._last_error:
            state["last_error"] = self._last_error

        msg = String()
        msg.data = json.dumps(state, separators=(",", ":"))
        self._publisher.publish(msg)


def main(args: list[str] | None = None) -> None:
    """ROS entrypoint."""
    rclpy.init(args=args)
    node = NavStatePublisher()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, SystemExit):
        pass
    finally:
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
