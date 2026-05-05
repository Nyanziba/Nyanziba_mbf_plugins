#!/usr/bin/env python3
"""rviz2 "2D Goal Pose" -> mbf MoveBase action bridge.

rviz2 の標準ツール **2D Goal Pose** はクリック位置を
``geometry_msgs/PoseStamped`` として ``/goal_pose`` に発行する。本ノードは
それを購読し、選択中の planner / controller を添えて
``/move_base_flex/move_base`` (mbf MoveBase action) に転送する。これにより
rviz 側にプラグインを書かなくても **Send Goal でアクション実行** が成立する。

Parameters:
    goal_topic       str    既定 "/goal_pose" — rviz2 が発行する topic
    action_name      str    既定 "/move_base_flex/move_base"
    planner          str    既定 "astar"
    controller       str    既定 "lookahead"
    cancel_on_new    bool   既定 True — 新しいゴール到着時に進行中をキャンセル
    server_wait_sec  float  既定 30.0 — 起動時にアクションサーバを待つ最大秒

Usage:
    ros2 run texnitis_mbf_tools rviz_goal_bridge.py
    # rviz2 で /goal_pose を発行する任意のツール（"2D Goal Pose" など）から発火
"""

from __future__ import annotations

from typing import Any, Optional

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped
from rclpy.action import ActionClient
from rclpy.node import Node

from mbf_msgs.action import MoveBase


class RvizGoalBridge(Node):
    """Forward each `/goal_pose` PoseStamped as an mbf MoveBase goal."""

    def __init__(self) -> None:
        super().__init__("rviz_goal_bridge")

        self.declare_parameter("goal_topic", "/goal_pose")
        self.declare_parameter("action_name", "/move_base_flex/move_base")
        self.declare_parameter("planner", "astar")
        self.declare_parameter("controller", "lookahead")
        self.declare_parameter("cancel_on_new", True)
        self.declare_parameter("server_wait_sec", 30.0)

        goal_topic = self.get_parameter("goal_topic").value
        action_name = self.get_parameter("action_name").value
        self._planner = self.get_parameter("planner").value
        self._controller = self.get_parameter("controller").value
        self._cancel_on_new = bool(self.get_parameter("cancel_on_new").value)
        server_wait_sec = float(self.get_parameter("server_wait_sec").value)

        self._client = ActionClient(self, MoveBase, action_name)
        self._active_goal_handle: Any = None

        self.get_logger().info(
            f"waiting up to {server_wait_sec:.0f}s for action server {action_name}..."
        )
        if not self._client.wait_for_server(timeout_sec=server_wait_sec):
            self.get_logger().warn(
                f"action server {action_name} not yet available; will keep retrying "
                "on each /goal_pose."
            )

        self._sub = self.create_subscription(
            PoseStamped, goal_topic, self._on_goal_pose, 1
        )
        self.get_logger().info(
            f"bridging {goal_topic} -> {action_name} "
            f"(planner={self._planner}, controller={self._controller}, "
            f"cancel_on_new={self._cancel_on_new})"
        )

    def _on_goal_pose(self, msg: PoseStamped) -> None:
        if self._cancel_on_new and self._active_goal_handle is not None:
            self.get_logger().info("cancelling previous goal due to new /goal_pose")
            try:
                cancel_future = self._active_goal_handle.cancel_goal_async()
                cancel_future.add_done_callback(self._on_cancel_done)
            except Exception as exc:  # noqa: BLE001
                self.get_logger().warn(f"cancel_goal_async raised: {exc!r}")
            self._active_goal_handle = None

        if not self._client.server_is_ready():
            if not self._client.wait_for_server(timeout_sec=1.0):
                self.get_logger().error(
                    "action server still unavailable; dropping this goal"
                )
                return

        goal = MoveBase.Goal()
        goal.target_pose = msg
        goal.planner = self._planner
        goal.controller = self._controller

        x = msg.pose.position.x
        y = msg.pose.position.y
        self.get_logger().info(
            f"forwarding /goal_pose (x={x:.2f}, y={y:.2f}, frame={msg.header.frame_id}) "
            f"-> {self._planner}/{self._controller}"
        )
        send_future = self._client.send_goal_async(goal)
        send_future.add_done_callback(self._on_goal_accepted)

    def _on_goal_accepted(self, future: Any) -> None:
        goal_handle = future.result()
        if goal_handle is None or not goal_handle.accepted:
            self.get_logger().error("mbf rejected the goal")
            return
        self._active_goal_handle = goal_handle
        result_future = goal_handle.get_result_async()
        result_future.add_done_callback(self._on_goal_result)

    def _on_goal_result(self, future: Any) -> None:
        wrapped = future.result()
        outcome = wrapped.result.outcome
        message = wrapped.result.message or ""
        status = wrapped.status
        if status == GoalStatus.STATUS_SUCCEEDED and outcome == 0:
            self.get_logger().info(
                f"goal SUCCEEDED (outcome=0, message={message!r})"
            )
        else:
            self.get_logger().warn(
                f"goal finished status={status} outcome={outcome} message={message!r}"
            )
        self._active_goal_handle = None

    def _on_cancel_done(self, future: Any) -> None:
        try:
            response = future.result()
        except Exception as exc:  # noqa: BLE001
            self.get_logger().warn(f"cancel future raised: {exc!r}")
            return
        accepted = getattr(response, "return_code", None)
        self.get_logger().info(f"previous goal cancel response code={accepted}")


def main(args: Optional[list[str]] = None) -> None:
    """ROS entrypoint."""
    rclpy.init(args=args)
    node = RvizGoalBridge()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, SystemExit):
        pass
    finally:
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
