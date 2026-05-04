#!/usr/bin/env python3
"""Demo: mbf MoveBase アクションに 1 ゴールだけ送って outcome を表示する.

実行:
    python3 demo/scripts/send_single_goal.py
    python3 demo/scripts/send_single_goal.py --x 3.0 --y 2.0 --yaw 1.57
"""

from __future__ import annotations

import argparse
import math
import sys

import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node

from geometry_msgs.msg import PoseStamped
from mbf_msgs.action import MoveBase


class GoalSender(Node):
    """1 つの goal を送って result を待つだけの最小ノード."""

    def __init__(self, x: float, y: float, yaw: float,
                 planner: str, controller: str, action_name: str) -> None:
        super().__init__("demo_goal_sender")
        self._client = ActionClient(self, MoveBase, action_name)
        self._planner = planner
        self._controller = controller
        self._goal = self._make_pose(x, y, yaw)
        self._exit_code: int = 1
        self._send()

    def _make_pose(self, x: float, y: float, yaw: float) -> PoseStamped:
        pose = PoseStamped()
        pose.header.frame_id = "map"
        pose.header.stamp = self.get_clock().now().to_msg()
        pose.pose.position.x = x
        pose.pose.position.y = y
        pose.pose.orientation.z = math.sin(yaw / 2.0)
        pose.pose.orientation.w = math.cos(yaw / 2.0)
        return pose

    def _send(self) -> None:
        self.get_logger().info(
            f"sending goal x={self._goal.pose.position.x:.2f} "
            f"y={self._goal.pose.position.y:.2f}"
        )
        self._client.wait_for_server(timeout_sec=10.0)
        if not self._client.server_is_ready():
            self.get_logger().error("mbf action server not available")
            self._exit_code = 1
            return
        goal = MoveBase.Goal()
        goal.target_pose = self._goal
        goal.planner = self._planner
        goal.controller = self._controller
        self._client.send_goal_async(goal).add_done_callback(self._on_accepted)

    def _on_accepted(self, future) -> None:
        handle = future.result()
        if not handle.accepted:
            self.get_logger().error("goal rejected")
            self._exit_code = 2
            rclpy.shutdown()
            return
        handle.get_result_async().add_done_callback(self._on_result)

    def _on_result(self, future) -> None:
        result = future.result().result
        self.get_logger().info(
            f"outcome={result.outcome} message={result.message!r}"
        )
        self._exit_code = 0 if result.outcome == 0 else 1
        rclpy.shutdown()

    @property
    def exit_code(self) -> int:
        """Return code propagated to the OS."""
        return self._exit_code


def main(argv: list[str] | None = None) -> int:
    """Entry point."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--x", type=float, default=2.0)
    parser.add_argument("--y", type=float, default=1.0)
    parser.add_argument("--yaw", type=float, default=0.0)
    parser.add_argument("--planner", default="astar")
    parser.add_argument("--controller", default="lookahead")
    parser.add_argument("--action", default="/move_base_flex/move_base")
    args = parser.parse_args(argv if argv is not None else sys.argv[1:])

    rclpy.init()
    node = GoalSender(args.x, args.y, args.yaw, args.planner,
                      args.controller, args.action)
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, SystemExit):
        pass
    return node.exit_code


if __name__ == "__main__":
    raise SystemExit(main())
