#!/usr/bin/env python3
"""Waypoint sender for the texnitis mbf navigation stack.

Drives the move_base_flex `MoveBase` action with a YAML-defined list of
waypoints. The YAML format is identical to the legacy
`texnitis_move_base_like` waypoint sender so existing files keep working:

```yaml
frame_id: "map"
loop: false
waypoints:
  - {x: 1.0, y: 0.0, yaw: 0.0}
  - {x: 2.0, y: 1.0, yaw: 1.57}
```

Usage:
  ros2 run texnitis_mbf_tools waypoint_sender.py \
      --ros-args -p waypoints_file:=path/to/waypoints.yaml
"""

from __future__ import annotations

from typing import Any

import rclpy
from geometry_msgs.msg import PoseStamped
from rclpy.action import ActionClient
from rclpy.node import Node

from mbf_msgs.action import MoveBase

from texnitis_mbf_tools.utils import load_waypoints_file, yaw_to_quaternion


class WaypointSender(Node):
    """Sequentially send waypoints to mbf's MoveBase action server."""

    def __init__(self) -> None:
        super().__init__("waypoint_sender")
        self.declare_parameter("waypoints_file", "")
        self.declare_parameter("action_name", "/move_base_flex/move_base")
        self.declare_parameter("planner", "astar")
        self.declare_parameter("controller", "lookahead")

        waypoints_file = self.get_parameter("waypoints_file").get_parameter_value().string_value
        action_name = self.get_parameter("action_name").get_parameter_value().string_value
        self._planner = self.get_parameter("planner").get_parameter_value().string_value
        self._controller = self.get_parameter("controller").get_parameter_value().string_value

        if not waypoints_file:
            self.get_logger().fatal("waypoints_file parameter is required")
            raise SystemExit(1)

        self._waypoints, self._frame_id, self._loop = load_waypoints_file(waypoints_file)
        self.get_logger().info(
            f"Loaded {len(self._waypoints)} waypoints (frame={self._frame_id} loop={self._loop})"
        )

        self._client = ActionClient(self, MoveBase, action_name)
        self._index = 0
        self._goal_handle: Any = None

        self.get_logger().info(f"Waiting for action server {action_name}...")
        self._client.wait_for_server()
        self.get_logger().info("Action server available. Sending first waypoint.")
        self._send_next()

    def _send_next(self) -> None:
        if self._index >= len(self._waypoints):
            if self._loop:
                self._index = 0
            else:
                self.get_logger().info("All waypoints reached. Shutting down.")
                self.destroy_node()
                rclpy.shutdown()
                return

        wp = self._waypoints[self._index]
        goal = MoveBase.Goal()
        goal.target_pose = self._make_pose_stamped(wp)
        goal.planner = self._planner
        goal.controller = self._controller

        self.get_logger().info(
            f"[{self._index + 1}/{len(self._waypoints)}] "
            f"sending x={wp['x']:.2f} y={wp['y']:.2f} yaw={wp['yaw']:.2f}"
        )

        future = self._client.send_goal_async(goal)
        future.add_done_callback(self._on_goal_accepted)

    def _make_pose_stamped(self, wp: dict[str, float]) -> PoseStamped:
        pose = PoseStamped()
        pose.header.frame_id = self._frame_id
        pose.header.stamp = self.get_clock().now().to_msg()
        pose.pose.position.x = wp["x"]
        pose.pose.position.y = wp["y"]
        pose.pose.position.z = 0.0
        qx, qy, qz, qw = yaw_to_quaternion(wp["yaw"])
        pose.pose.orientation.x = qx
        pose.pose.orientation.y = qy
        pose.pose.orientation.z = qz
        pose.pose.orientation.w = qw
        return pose

    def _on_goal_accepted(self, future: Any) -> None:
        goal_handle = future.result()
        if not goal_handle.accepted:
            self.get_logger().warn("Goal rejected by mbf; advancing to next waypoint")
            self._index += 1
            self._send_next()
            return
        self._goal_handle = goal_handle
        result_future = goal_handle.get_result_async()
        result_future.add_done_callback(self._on_goal_result)

    def _on_goal_result(self, future: Any) -> None:
        result = future.result()
        outcome = result.result.outcome
        message = result.result.message or ""
        if outcome == 0:
            self.get_logger().info(
                f"[{self._index + 1}/{len(self._waypoints)}] reached "
                f"(outcome={outcome} message={message!r})"
            )
        else:
            self.get_logger().warn(
                f"[{self._index + 1}/{len(self._waypoints)}] failed "
                f"(outcome={outcome} message={message!r})"
            )
        self._index += 1
        self._send_next()


def main(args: list[str] | None = None) -> None:
    """ROS entrypoint. spin() blocks until the sender finishes / Ctrl-C."""
    rclpy.init(args=args)
    node = WaypointSender()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, SystemExit):
        pass
    finally:
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
