#!/usr/bin/env python3
"""End-to-end navigation check used both interactively and from CI.

Sends one MoveBase goal to mbf, waits for the result, then exits with
status 0 on outcome=SUCCESS and 1 otherwise. Designed to be the foreground
process of a `ros2 launch ...` command in CI: the launch file should set
`on_exit=Shutdown()` for this node so launching brings the entire stack
down once the check is done.

Parameters:
    action_name        str    default "/move_base_flex/move_base"
    goal               list   [x, y, yaw]   default [3.0, 0.5, 0.0]
    frame_id           str    default "map"
    planner            str    default "astar"
    controller         str    default "lookahead"
    server_wait_sec    float  default 30.0
    overall_timeout_s  float  default 90.0
"""

from __future__ import annotations

import math
import sys
from typing import Optional

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped
from rclpy.action import ActionClient
from rclpy.node import Node

from mbf_msgs.action import MoveBase


def yaw_to_quaternion(yaw: float) -> tuple[float, float, float, float]:
    return (0.0, 0.0, math.sin(yaw / 2.0), math.cos(yaw / 2.0))


class E2EChecker(Node):
    def __init__(self) -> None:
        super().__init__("mbf_e2e_check")

        self.declare_parameter("action_name", "/move_base_flex/move_base")
        self.declare_parameter("goal", [3.0, 0.5, 0.0])
        self.declare_parameter("frame_id", "map")
        self.declare_parameter("planner", "astar")
        self.declare_parameter("controller", "lookahead")
        self.declare_parameter("server_wait_sec", 30.0)
        self.declare_parameter("overall_timeout_s", 90.0)

        self._action_name = self.get_parameter("action_name").value
        goal = list(self.get_parameter("goal").value)
        if len(goal) != 3:
            self.get_logger().fatal(f"goal expects [x,y,yaw], got {goal}")
            raise SystemExit(2)
        self._goal_xyyaw = (float(goal[0]), float(goal[1]), float(goal[2]))
        self._frame_id = self.get_parameter("frame_id").value
        self._planner = self.get_parameter("planner").value
        self._controller = self.get_parameter("controller").value
        self._server_wait_sec = float(self.get_parameter("server_wait_sec").value)
        self._overall_timeout_s = float(self.get_parameter("overall_timeout_s").value)

        self._client = ActionClient(self, MoveBase, self._action_name)
        self._exit_code: Optional[int] = None
        self._status_label = "pending"

    def run(self) -> int:
        self.get_logger().info(
            f"waiting up to {self._server_wait_sec:.0f}s for action server "
            f"{self._action_name}..."
        )
        if not self._client.wait_for_server(timeout_sec=self._server_wait_sec):
            self.get_logger().error("action server never came up")
            return 2

        goal_msg = MoveBase.Goal()
        goal_msg.target_pose = self._make_pose_stamped(*self._goal_xyyaw)
        goal_msg.planner = self._planner
        goal_msg.controller = self._controller

        self.get_logger().info(
            f"sending goal x={self._goal_xyyaw[0]:.2f} y={self._goal_xyyaw[1]:.2f} "
            f"yaw={self._goal_xyyaw[2]:.2f} (planner={self._planner}, "
            f"controller={self._controller})"
        )
        send_future = self._client.send_goal_async(goal_msg)
        rclpy.spin_until_future_complete(self, send_future, timeout_sec=10.0)
        if not send_future.done():
            self.get_logger().error("send_goal_async never returned")
            return 3
        goal_handle = send_future.result()
        if goal_handle is None or not goal_handle.accepted:
            self.get_logger().error("goal rejected by mbf")
            return 4

        result_future = goal_handle.get_result_async()
        rclpy.spin_until_future_complete(
            self, result_future, timeout_sec=self._overall_timeout_s
        )

        if not result_future.done():
            self.get_logger().error(
                f"goal did not finish within {self._overall_timeout_s:.0f}s; "
                "cancelling and failing the check"
            )
            cancel_future = goal_handle.cancel_goal_async()
            rclpy.spin_until_future_complete(self, cancel_future, timeout_sec=5.0)
            return 5

        wrapped = result_future.result()
        outcome = wrapped.result.outcome
        message = wrapped.result.message or ""
        status = wrapped.status

        if status == GoalStatus.STATUS_SUCCEEDED and outcome == 0:
            self.get_logger().info(
                f"E2E PASS (outcome=0, status=SUCCEEDED, message={message!r})"
            )
            return 0
        self.get_logger().error(
            f"E2E FAIL outcome={outcome} status={status} message={message!r}"
        )
        return 1

    def _make_pose_stamped(self, x: float, y: float, yaw: float) -> PoseStamped:
        pose = PoseStamped()
        pose.header.frame_id = self._frame_id
        pose.header.stamp = self.get_clock().now().to_msg()
        pose.pose.position.x = x
        pose.pose.position.y = y
        qx, qy, qz, qw = yaw_to_quaternion(yaw)
        pose.pose.orientation.x = qx
        pose.pose.orientation.y = qy
        pose.pose.orientation.z = qz
        pose.pose.orientation.w = qw
        return pose


def main(args: list[str] | None = None) -> None:
    rclpy.init(args=args)
    checker = E2EChecker()
    try:
        exit_code = checker.run()
    finally:
        checker.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
