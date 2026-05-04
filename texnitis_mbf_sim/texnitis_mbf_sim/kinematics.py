"""Pure-Python 2D kinematic integrator used by the flat-world sim node.

We deliberately stay rclpy-free here so the integrator can be unit tested
without a ROS install. The model is a holonomic body (matches mecanum):
the commanded body-frame twist `(vx, vy, wz)` is integrated using a
midpoint heading so the geometric error is O(dt^2). For the diff-drive
case the bringup yaml sets `vy=0`, so the same integrator works.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


@dataclass
class HolonomicState:
    """Pose of the simulated body in the world frame (SE(2))."""

    x: float = 0.0
    y: float = 0.0
    yaw: float = 0.0


def integrate_holonomic(
    state: HolonomicState,
    body_vx: float,
    body_vy: float,
    body_wz: float,
    dt: float,
) -> HolonomicState:
    """Advance pose by `dt` under a constant body twist.

    Uses the midpoint heading (yaw + 0.5 * wz * dt) to reduce drift versus
    forward Euler. dt must be non-negative; non-positive dt is a no-op so
    callers can pass jittered wall-clock differences without guarding.
    """
    if dt <= 0.0:
        return HolonomicState(x=state.x, y=state.y, yaw=state.yaw)

    midpoint_yaw = state.yaw + 0.5 * body_wz * dt
    cos_yaw = math.cos(midpoint_yaw)
    sin_yaw = math.sin(midpoint_yaw)

    world_vx = cos_yaw * body_vx - sin_yaw * body_vy
    world_vy = sin_yaw * body_vx + cos_yaw * body_vy

    new_yaw = state.yaw + body_wz * dt
    new_yaw = math.atan2(math.sin(new_yaw), math.cos(new_yaw))

    return HolonomicState(
        x=state.x + world_vx * dt,
        y=state.y + world_vy * dt,
        yaw=new_yaw,
    )
