#!/usr/bin/env python3
"""Legacy ``SetGoal.srv`` -> mbf MoveBase action bridge.

For migration only: lets old clients keep using
``/move_base_like_node/set_goal`` while the goal is forwarded to the
mbf action server. Once every consumer has moved to the action API
this script is no longer needed.

Activation: this script imports the legacy
``texnitis_move_base_like.srv.SetGoal`` type. If that package is not
present in the workspace (because the legacy package has been
removed), the script logs a fatal and exits.
"""

from __future__ import annotations

import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node

from mbf_msgs.action import MoveBase

try:
    from texnitis_move_base_like.srv import SetGoal  # type: ignore
except ImportError:  # pragma: no cover - guarded for post-deletion environments
    SetGoal = None  # type: ignore


class MbfActionBridge(Node):
    """Forwards legacy SetGoal calls to the mbf MoveBase action server."""

    def __init__(self) -> None:
        super().__init__("mbf_action_bridge")
        if SetGoal is None:
            self.get_logger().fatal(
                "texnitis_move_base_like.srv.SetGoal is unavailable; "
                "this bridge is only useful while the legacy package is "
                "still in the workspace."
            )
            raise SystemExit(1)

        self.declare_parameter("service_name", "/move_base_like_node/set_goal")
        self.declare_parameter("action_name", "/move_base_flex/move_base")
        self.declare_parameter("planner", "astar")
        self.declare_parameter("controller", "lookahead")

        srv_name = self.get_parameter("service_name").value
        action_name = self.get_parameter("action_name").value
        self._planner = self.get_parameter("planner").value
        self._controller = self.get_parameter("controller").value

        self._client = ActionClient(self, MoveBase, action_name)
        self._client.wait_for_server()
        self._service = self.create_service(SetGoal, srv_name, self._handle_set_goal)
        self.get_logger().info(
            f"bridging {srv_name} -> {action_name} "
            f"(planner={self._planner} controller={self._controller})"
        )

    def _handle_set_goal(self, request, response):
        goal = MoveBase.Goal()
        goal.target_pose = request.goal
        goal.planner = self._planner
        goal.controller = self._controller
        future = self._client.send_goal_async(goal)
        # Don't wait synchronously — the bridge is fire-and-forget; clients
        # observe completion via mbf feedback / nav_state_publisher.
        response.accepted = True
        response.message = "forwarded to mbf"
        return response


def main(args: list[str] | None = None) -> None:
    """ROS entrypoint."""
    rclpy.init(args=args)
    node = MbfActionBridge()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, SystemExit):
        pass
    finally:
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
