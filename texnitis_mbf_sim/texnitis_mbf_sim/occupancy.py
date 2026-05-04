"""Minimal occupancy grid wrapper used for collision detection in the sim.

We do not depend on `nav_msgs/OccupancyGrid` here so the file stays unit
testable without a ROS install. The `flat_world_sim_node` adapts a live
OccupancyGrid into this struct in O(1) (zero-copy by reference to the
underlying buffer).
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Sequence


@dataclass
class OccupancyMap:
    """Occupancy grid in map frame (origin at lower-left, +x right, +y up).

    `data` is row-major with row 0 = world y == origin_y. Cell values follow
    the ROS convention: -1 unknown, 0 free, 100 lethal. We treat anything
    `>= occupied_threshold` (default 65) as a collision.
    """

    width: int
    height: int
    resolution: float
    origin_x: float
    origin_y: float
    data: Sequence[int]
    occupied_threshold: int = 65

    def world_to_map(self, world_x: float, world_y: float) -> tuple[int, int]:
        col = int(math.floor((world_x - self.origin_x) / self.resolution))
        row = int(math.floor((world_y - self.origin_y) / self.resolution))
        return col, row

    def in_bounds(self, col: int, row: int) -> bool:
        return 0 <= col < self.width and 0 <= row < self.height

    def is_occupied(self, world_x: float, world_y: float) -> bool:
        """Return True if (world_x, world_y) lies in a lethal cell.

        Out-of-bounds is treated as occupied: it stops the robot from
        running off the map in CI.
        """
        col, row = self.world_to_map(world_x, world_y)
        if not self.in_bounds(col, row):
            return True
        value = self.data[row * self.width + col]
        return value >= self.occupied_threshold
