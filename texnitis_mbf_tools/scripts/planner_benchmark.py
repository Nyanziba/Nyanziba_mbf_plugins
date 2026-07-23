#!/usr/bin/env python3
"""Run alternating MBF planner trials and write raw CSV plus JSON summary.

Before every trial, place the robot at the same marked start pose. The node
waits for operator confirmation on the ``~/ready`` SetBool service, preventing
an accidental run while the course is occupied.
"""

from __future__ import annotations

import csv
import json
import math
from pathlib import Path
from time import monotonic
from typing import Any

import rclpy
from geometry_msgs.msg import PoseStamped
from mbf_msgs.action import MoveBase
from nav_msgs.msg import Odometry
from rclpy.action import ActionClient
from rclpy.node import Node
from std_srvs.srv import SetBool

from texnitis_mbf_tools.planner_benchmark import TrialRecord, summarize
from texnitis_mbf_tools.utils import yaw_to_quaternion


class PlannerBenchmark(Node):
    def __init__(self) -> None:
        super().__init__("planner_benchmark")
        self.declare_parameter("planners", ["astar", "kinematic_time_astar"])
        self.declare_parameter("trials_per_planner", 5)
        self.declare_parameter("controller", "lookahead")
        self.declare_parameter("action_name", "/move_base_flex/move_base")
        self.declare_parameter("odom_topic", "/odom")
        self.declare_parameter("frame_id", "map")
        self.declare_parameter("goal_x", 1.0)
        self.declare_parameter("goal_y", 0.0)
        self.declare_parameter("goal_yaw", 0.0)
        self.declare_parameter("output_prefix", "planner_benchmark")
        planners = list(self.get_parameter("planners").value)
        trials = int(self.get_parameter("trials_per_planner").value)
        self._schedule = [(planner, trial + 1) for trial in range(trials) for planner in planners]
        self._records: list[TrialRecord] = []
        self._distance = 0.0
        self._last_xy: tuple[float, float] | None = None
        self._running = False
        self._started = 0.0
        self._client = ActionClient(self, MoveBase, str(self.get_parameter("action_name").value))
        self.create_subscription(Odometry, str(self.get_parameter("odom_topic").value), self._on_odom, 20)
        self.create_service(SetBool, "~/ready", self._on_ready)
        self.get_logger().info("Waiting for MBF action server")
        self._client.wait_for_server()
        self.get_logger().info("Reset robot to the marked start, then call: ros2 service call ~/ready std_srvs/srv/SetBool '{data: true}'")

    def _on_odom(self, msg: Odometry) -> None:
        xy = (msg.pose.pose.position.x, msg.pose.pose.position.y)
        if self._running and self._last_xy is not None:
            self._distance += math.hypot(xy[0] - self._last_xy[0], xy[1] - self._last_xy[1])
        self._last_xy = xy

    def _on_ready(self, request: SetBool.Request, response: SetBool.Response) -> SetBool.Response:
        if not request.data or self._running or not self._schedule:
            response.success = False
            response.message = "not ready, already running, or benchmark complete"
            return response
        planner, trial = self._schedule.pop(0)
        goal = MoveBase.Goal()
        goal.planner = planner
        goal.controller = str(self.get_parameter("controller").value)
        goal.target_pose = self._goal_pose()
        self._active = (planner, trial)
        self._distance = 0.0
        self._running = True
        self._started = monotonic()
        self._client.send_goal_async(goal).add_done_callback(self._accepted)
        response.success = True
        response.message = f"started {planner} trial {trial}"
        return response

    def _goal_pose(self) -> PoseStamped:
        pose = PoseStamped()
        pose.header.frame_id = str(self.get_parameter("frame_id").value)
        pose.header.stamp = self.get_clock().now().to_msg()
        pose.pose.position.x = float(self.get_parameter("goal_x").value)
        pose.pose.position.y = float(self.get_parameter("goal_y").value)
        q = yaw_to_quaternion(float(self.get_parameter("goal_yaw").value))
        pose.pose.orientation.x, pose.pose.orientation.y, pose.pose.orientation.z, pose.pose.orientation.w = q
        return pose

    def _accepted(self, future: Any) -> None:
        handle = future.result()
        if not handle.accepted:
            self._finish(255)
            return
        handle.get_result_async().add_done_callback(lambda result: self._finish(int(result.result().result.outcome)))

    def _finish(self, outcome: int) -> None:
        planner, trial = self._active
        self._records.append(TrialRecord(planner, trial, outcome == 0, monotonic() - self._started, self._distance, outcome))
        self._running = False
        self._write_results()
        if self._schedule:
            self.get_logger().info("Trial complete. Return robot to the identical start pose and call ~/ready again")
        else:
            self.get_logger().info("Benchmark complete")

    def _write_results(self) -> None:
        prefix = Path(str(self.get_parameter("output_prefix").value)).expanduser()
        with prefix.with_suffix(".csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(TrialRecord.__dataclass_fields__))
            writer.writeheader()
            writer.writerows(record.as_dict() for record in self._records)
        prefix.with_suffix(".json").write_text(json.dumps(summarize(self._records), indent=2), encoding="utf-8")


def main(args: list[str] | None = None) -> None:
    rclpy.init(args=args)
    node = PlannerBenchmark()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
