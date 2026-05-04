#!/usr/bin/env python3
"""Headless 2D world simulator for the mbf navigation stack.

Subscribes:
    /cmd_vel  (geometry_msgs/Twist)        body-frame command from mbf.
    /map      (nav_msgs/OccupancyGrid)     used for collision tracking.

Publishes:
    /odom     (nav_msgs/Odometry)          ground-truth pose (no noise).
    tf        map  -> odom                  identity (we publish odom == map).
    tf        odom -> base_link             current sim pose.

The simulator is intentionally noise-free and Gazebo-free so the same
node can run in CI containers without a GPU. Disturbance modelling
belongs in the nav_core Python sim; this node is a control-loop closer
for the ROS 2 stack only.

Parameters:
    base_frame              str   default "base_link"
    odom_frame              str   default "odom"
    map_frame               str   default "map"
    update_rate             float default 50.0  [Hz]
    max_linear_speed        float default 1.0   [m/s]   sat applied to /cmd_vel
    max_angular_speed       float default 2.0   [rad/s] sat applied to /cmd_vel
    initial_pose            list  default [0,0,0]      x,y,yaw in map frame
    publish_map_to_odom_tf  bool  default true
"""

from __future__ import annotations

import math
from typing import Optional

import rclpy
from geometry_msgs.msg import Twist, TransformStamped
from nav_msgs.msg import Odometry, OccupancyGrid
from rclpy.node import Node
from rclpy.qos import QoSDurabilityPolicy, QoSProfile, QoSReliabilityPolicy
from tf2_ros import TransformBroadcaster

from texnitis_mbf_sim.kinematics import HolonomicState, integrate_holonomic
from texnitis_mbf_sim.occupancy import OccupancyMap


def yaw_to_quaternion(yaw: float) -> tuple[float, float, float, float]:
    return (0.0, 0.0, math.sin(yaw / 2.0), math.cos(yaw / 2.0))


def saturate(value: float, limit: float) -> float:
    if limit <= 0.0:
        return value
    return max(-limit, min(limit, value))


class FlatWorldSimNode(Node):
    """Owns the simulated robot pose and republishes it as /odom + tf."""

    def __init__(self) -> None:
        super().__init__("flat_world_sim")

        self.declare_parameter("base_frame", "base_link")
        self.declare_parameter("odom_frame", "odom")
        self.declare_parameter("map_frame", "map")
        self.declare_parameter("update_rate", 50.0)
        self.declare_parameter("max_linear_speed", 1.0)
        self.declare_parameter("max_angular_speed", 2.0)
        self.declare_parameter("initial_pose", [0.0, 0.0, 0.0])
        self.declare_parameter("publish_map_to_odom_tf", True)
        self.declare_parameter("cmd_vel_topic", "/cmd_vel")
        self.declare_parameter("odom_topic", "/odom")
        self.declare_parameter("map_topic", "/map")

        self._base_frame = self.get_parameter("base_frame").value
        self._odom_frame = self.get_parameter("odom_frame").value
        self._map_frame = self.get_parameter("map_frame").value
        self._update_rate = float(self.get_parameter("update_rate").value)
        self._max_linear_speed = float(self.get_parameter("max_linear_speed").value)
        self._max_angular_speed = float(self.get_parameter("max_angular_speed").value)
        self._publish_map_to_odom_tf = bool(
            self.get_parameter("publish_map_to_odom_tf").value
        )

        initial_pose = list(self.get_parameter("initial_pose").value)
        if len(initial_pose) != 3:
            self.get_logger().warn(
                f"initial_pose expects [x, y, yaw], got {initial_pose}; using zeros."
            )
            initial_pose = [0.0, 0.0, 0.0]

        self._state = HolonomicState(
            x=float(initial_pose[0]),
            y=float(initial_pose[1]),
            yaw=float(initial_pose[2]),
        )
        self._cmd_vx = 0.0
        self._cmd_vy = 0.0
        self._cmd_wz = 0.0

        self._occupancy: Optional[OccupancyMap] = None
        self._collision_count = 0
        self._last_collision_log_step = -1
        self._step_count = 0

        cmd_vel_topic = self.get_parameter("cmd_vel_topic").value
        odom_topic = self.get_parameter("odom_topic").value
        map_topic = self.get_parameter("map_topic").value

        self._cmd_sub = self.create_subscription(
            Twist, cmd_vel_topic, self._on_cmd_vel, 10
        )

        map_qos = QoSProfile(
            depth=1,
            reliability=QoSReliabilityPolicy.RELIABLE,
            durability=QoSDurabilityPolicy.TRANSIENT_LOCAL,
        )
        self._map_sub = self.create_subscription(
            OccupancyGrid, map_topic, self._on_map, map_qos
        )

        self._odom_pub = self.create_publisher(Odometry, odom_topic, 10)
        self._tf_broadcaster = TransformBroadcaster(self)

        period = 1.0 / max(self._update_rate, 1.0)
        self._dt = period
        self._timer = self.create_timer(period, self._on_timer)

        self.get_logger().info(
            "flat_world_sim ready "
            f"(rate={self._update_rate:.1f}Hz, init=[{self._state.x:.2f},"
            f"{self._state.y:.2f},{self._state.yaw:.2f}])"
        )

    def _on_cmd_vel(self, msg: Twist) -> None:
        self._cmd_vx = saturate(msg.linear.x, self._max_linear_speed)
        self._cmd_vy = saturate(msg.linear.y, self._max_linear_speed)
        self._cmd_wz = saturate(msg.angular.z, self._max_angular_speed)

    def _on_map(self, msg: OccupancyGrid) -> None:
        self._occupancy = OccupancyMap(
            width=msg.info.width,
            height=msg.info.height,
            resolution=msg.info.resolution,
            origin_x=msg.info.origin.position.x,
            origin_y=msg.info.origin.position.y,
            data=list(msg.data),
        )
        self.get_logger().info(
            f"received map {msg.info.width}x{msg.info.height} "
            f"@ {msg.info.resolution:.3f} m/cell"
        )

    def _on_timer(self) -> None:
        self._step_count += 1
        previous = HolonomicState(self._state.x, self._state.y, self._state.yaw)

        self._state = integrate_holonomic(
            self._state,
            self._cmd_vx,
            self._cmd_vy,
            self._cmd_wz,
            self._dt,
        )

        if self._occupancy is not None and self._occupancy.is_occupied(
            self._state.x, self._state.y
        ):
            # Bounce back to the previous safe pose; mbf's controller
            # should respond to the lack of motion. We log every 1 s
            # at most so the test output stays readable.
            self._collision_count += 1
            if self._step_count - self._last_collision_log_step > self._update_rate:
                self.get_logger().warn(
                    f"collision at ({self._state.x:.2f},{self._state.y:.2f}); "
                    f"holding previous pose (count={self._collision_count})"
                )
                self._last_collision_log_step = self._step_count
            self._state = previous

        now = self.get_clock().now().to_msg()
        self._publish_odom(now)
        self._publish_tfs(now)

    def _publish_odom(self, stamp) -> None:
        msg = Odometry()
        msg.header.stamp = stamp
        msg.header.frame_id = self._odom_frame
        msg.child_frame_id = self._base_frame
        msg.pose.pose.position.x = self._state.x
        msg.pose.pose.position.y = self._state.y
        qx, qy, qz, qw = yaw_to_quaternion(self._state.yaw)
        msg.pose.pose.orientation.x = qx
        msg.pose.pose.orientation.y = qy
        msg.pose.pose.orientation.z = qz
        msg.pose.pose.orientation.w = qw
        msg.twist.twist.linear.x = self._cmd_vx
        msg.twist.twist.linear.y = self._cmd_vy
        msg.twist.twist.angular.z = self._cmd_wz
        self._odom_pub.publish(msg)

    def _publish_tfs(self, stamp) -> None:
        if self._publish_map_to_odom_tf:
            map_to_odom = TransformStamped()
            map_to_odom.header.stamp = stamp
            map_to_odom.header.frame_id = self._map_frame
            map_to_odom.child_frame_id = self._odom_frame
            map_to_odom.transform.rotation.w = 1.0
            self._tf_broadcaster.sendTransform(map_to_odom)

        odom_to_base = TransformStamped()
        odom_to_base.header.stamp = stamp
        odom_to_base.header.frame_id = self._odom_frame
        odom_to_base.child_frame_id = self._base_frame
        odom_to_base.transform.translation.x = self._state.x
        odom_to_base.transform.translation.y = self._state.y
        qx, qy, qz, qw = yaw_to_quaternion(self._state.yaw)
        odom_to_base.transform.rotation.x = qx
        odom_to_base.transform.rotation.y = qy
        odom_to_base.transform.rotation.z = qz
        odom_to_base.transform.rotation.w = qw
        self._tf_broadcaster.sendTransform(odom_to_base)


def main(args: list[str] | None = None) -> None:
    rclpy.init(args=args)
    node = FlatWorldSimNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
