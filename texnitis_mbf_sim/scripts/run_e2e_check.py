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
import os
import sys
from typing import Optional

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped, TwistStamped
from nav_msgs.msg import Odometry
from rclpy.action import ActionClient
from rclpy.node import Node

from mbf_msgs.action import MoveBase


def yaw_to_quaternion(yaw: float) -> tuple[float, float, float, float]:
    return (0.0, 0.0, math.sin(yaw / 2.0), math.cos(yaw / 2.0))


class E2EChecker(Node):
    def __init__(self) -> None:
        super().__init__("mbf_e2e_check")

        self.declare_parameter("action_name", "/move_base_flex/move_base")
        # Three scalar params instead of one DOUBLE_ARRAY: launch's
        # LaunchConfiguration substitutions cannot be packed into a list
        # parameter without being concatenated as a single string, so we
        # accept them separately and reconstruct the tuple here.
        self.declare_parameter("goal_x", 3.0)
        self.declare_parameter("goal_y", 0.5)
        self.declare_parameter("goal_yaw", 0.0)
        self.declare_parameter("frame_id", "map")
        self.declare_parameter("planner", "astar")
        self.declare_parameter("controller", "lookahead")
        self.declare_parameter("server_wait_sec", 30.0)
        self.declare_parameter("overall_timeout_s", 90.0)

        self._action_name = self.get_parameter("action_name").value
        self._goal_xyyaw = (
            float(self.get_parameter("goal_x").value),
            float(self.get_parameter("goal_y").value),
            float(self.get_parameter("goal_yaw").value),
        )
        self._frame_id = self.get_parameter("frame_id").value
        self._planner = self.get_parameter("planner").value
        self._controller = self.get_parameter("controller").value
        self._server_wait_sec = float(self.get_parameter("server_wait_sec").value)
        self._overall_timeout_s = float(self.get_parameter("overall_timeout_s").value)

        self._client = ActionClient(self, MoveBase, self._action_name)
        self._exit_code: Optional[int] = None
        self._status_label = "pending"

        # Track odometry so we can confirm the sim node has started
        # publishing TF before sending a goal. mbf rejects goals when
        # the map -> base_link lookup is younger than the goal stamp.
        self._got_odom = False
        self._latest_pose: Optional[tuple[float, float, float]] = None
        self._cmd_vel_count = 0
        self._latest_cmd_vel: Optional[tuple[float, float, float]] = None
        self._diag_timer: Optional[Any] = None
        self._odom_sub = self.create_subscription(
            Odometry, "/odom", self._on_odom, 10
        )
        self._cmd_vel_sub = self.create_subscription(
            TwistStamped, "/cmd_vel", self._on_cmd_vel, 10
        )

    def _on_odom(self, msg: Odometry) -> None:
        self._got_odom = True
        self._latest_pose = (
            msg.pose.pose.position.x,
            msg.pose.pose.position.y,
            msg.pose.pose.orientation.z,
        )

    def _on_cmd_vel(self, msg: TwistStamped) -> None:
        self._cmd_vel_count += 1
        self._latest_cmd_vel = (msg.twist.linear.x, msg.twist.linear.y, msg.twist.angular.z)

    def _on_diag(self) -> None:
        pose = self._latest_pose or (0.0, 0.0, 0.0)
        cmd = self._latest_cmd_vel or (0.0, 0.0, 0.0)
        self.get_logger().info(
            f"[diag] pose=({pose[0]:.2f},{pose[1]:.2f}) "
            f"cmd_vel_count={self._cmd_vel_count} "
            f"last_cmd=(vx={cmd[0]:.2f}, vy={cmd[1]:.2f}, wz={cmd[2]:.2f})"
        )

    def _wait_for_odom(self, timeout_sec: float) -> bool:
        deadline = self.get_clock().now().nanoseconds + int(timeout_sec * 1e9)
        while not self._got_odom and self.get_clock().now().nanoseconds < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)
        return self._got_odom

    def run(self) -> int:
        self.get_logger().info(
            f"waiting up to {self._server_wait_sec:.0f}s for action server "
            f"{self._action_name}..."
        )
        if not self._client.wait_for_server(timeout_sec=self._server_wait_sec):
            self.get_logger().error("action server never came up")
            return 2

        # Block until /odom is flowing (sim node up). mbf's RobotInfo only
        # opens its own /odom subscription after its action is created, so
        # we additionally wait for several seconds of warmup so the tf
        # buffer in mbf has samples bracketing the goal stamp.
        if not self._wait_for_odom(timeout_sec=15.0):
            self.get_logger().error("no /odom seen within 15s; sim node not running?")
            return 6
        warmup_seconds = 5.0
        self.get_logger().info(
            f"odom flowing; warming up tf buffer for {warmup_seconds:.1f}s before sending goal"
        )
        end_warmup = self.get_clock().now().nanoseconds + int(warmup_seconds * 1e9)
        while self.get_clock().now().nanoseconds < end_warmup:
            rclpy.spin_once(self, timeout_sec=0.05)

        goal_msg = MoveBase.Goal()
        goal_msg.target_pose = self._make_pose_stamped(*self._goal_xyyaw)
        goal_msg.planner = self._planner
        goal_msg.controller = self._controller

        self.get_logger().info(
            f"sending goal x={self._goal_xyyaw[0]:.2f} y={self._goal_xyyaw[1]:.2f} "
            f"yaw={self._goal_xyyaw[2]:.2f} (planner={self._planner}, "
            f"controller={self._controller})"
        )
        # Heartbeat: every 5 s log pose + cmd_vel count so a stuck
        # controller leaves a trail in CI logs instead of going dark
        # for the whole timeout window.
        self._diag_timer = self.create_timer(5.0, self._on_diag)
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

    # ROS 2 launch's Shutdown event handler swallows process exit codes, so
    # a non-zero return here would still surface as `ros2 launch` exit 0.
    # Persist the verdict to a well-known file so CI can verify it after
    # the launch returns.
    sentinel_path = os.environ.get("MBF_E2E_RESULT_FILE")
    if sentinel_path:
        try:
            with open(sentinel_path, "w") as fp:
                fp.write(str(exit_code))
        except OSError:
            # CI will treat a missing file as failure, which is correct.
            pass
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
