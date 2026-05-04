"""Unit tests for the holonomic integrator and occupancy collision check."""

from __future__ import annotations

import math

import pytest

from texnitis_mbf_sim.kinematics import HolonomicState, integrate_holonomic
from texnitis_mbf_sim.occupancy import OccupancyMap


class TestIntegrateHolonomic:
    def test_zero_dt_is_identity(self) -> None:
        start = HolonomicState(1.0, 2.0, 0.5)
        end = integrate_holonomic(start, 1.0, 0.0, 0.0, 0.0)
        assert end.x == pytest.approx(1.0)
        assert end.y == pytest.approx(2.0)
        assert end.yaw == pytest.approx(0.5)

    def test_negative_dt_is_identity(self) -> None:
        start = HolonomicState(0.0, 0.0, 0.0)
        end = integrate_holonomic(start, 1.0, 1.0, 1.0, -0.1)
        assert end.x == 0.0
        assert end.y == 0.0
        assert end.yaw == 0.0

    def test_pure_forward_motion_in_world_frame(self) -> None:
        start = HolonomicState(0.0, 0.0, 0.0)
        end = integrate_holonomic(start, 1.0, 0.0, 0.0, 1.0)
        assert end.x == pytest.approx(1.0)
        assert end.y == pytest.approx(0.0)
        assert end.yaw == pytest.approx(0.0)

    def test_forward_motion_under_heading(self) -> None:
        start = HolonomicState(0.0, 0.0, math.pi / 2)
        end = integrate_holonomic(start, 1.0, 0.0, 0.0, 1.0)
        assert end.x == pytest.approx(0.0, abs=1e-9)
        assert end.y == pytest.approx(1.0)

    def test_lateral_velocity_in_body_frame(self) -> None:
        start = HolonomicState(0.0, 0.0, 0.0)
        end = integrate_holonomic(start, 0.0, 1.0, 0.0, 1.0)
        assert end.x == pytest.approx(0.0, abs=1e-9)
        assert end.y == pytest.approx(1.0)

    def test_yaw_wraps_into_minus_pi_pi(self) -> None:
        start = HolonomicState(0.0, 0.0, 3.0)
        end = integrate_holonomic(start, 0.0, 0.0, 1.0, 1.0)
        assert -math.pi <= end.yaw <= math.pi

    def test_circle_motion_returns_to_start_after_full_period(self) -> None:
        # vx = R * w, vy = 0 ; expect a circle of radius R = 1 m
        radius = 1.0
        omega = 1.0
        forward_speed = radius * omega
        period = 2.0 * math.pi / omega

        state = HolonomicState(0.0, 0.0, 0.0)
        dt = 0.001
        steps = int(round(period / dt))
        for _ in range(steps):
            state = integrate_holonomic(state, forward_speed, 0.0, omega, dt)

        # midpoint integration should bring us back near origin
        assert state.x == pytest.approx(0.0, abs=1e-2)
        assert state.y == pytest.approx(0.0, abs=1e-2)


class TestOccupancyMap:
    def _make_open_map(self) -> OccupancyMap:
        return OccupancyMap(
            width=4,
            height=4,
            resolution=1.0,
            origin_x=0.0,
            origin_y=0.0,
            data=[0] * 16,
        )

    def test_in_bounds_free_cell(self) -> None:
        m = self._make_open_map()
        assert m.is_occupied(2.5, 2.5) is False

    def test_out_of_bounds_treated_as_occupied(self) -> None:
        m = self._make_open_map()
        assert m.is_occupied(-1.0, 0.5) is True
        assert m.is_occupied(0.5, 100.0) is True

    def test_lethal_cell_detected(self) -> None:
        data = [0] * 16
        # row 1 col 2 -> world (2.5, 1.5)
        data[1 * 4 + 2] = 100
        m = OccupancyMap(4, 4, 1.0, 0.0, 0.0, data)
        assert m.is_occupied(2.5, 1.5) is True
        assert m.is_occupied(0.5, 0.5) is False

    def test_threshold_boundary(self) -> None:
        data = [0] * 16
        data[0] = 64  # below default 65 -> free
        data[1] = 65  # at default threshold -> lethal
        m = OccupancyMap(4, 4, 1.0, 0.0, 0.0, data)
        assert m.is_occupied(0.5, 0.5) is False
        assert m.is_occupied(1.5, 0.5) is True

    def test_world_to_map_origin_offset(self) -> None:
        m = OccupancyMap(4, 4, 1.0, -2.0, -2.0, [0] * 16)
        col, row = m.world_to_map(-2.0, -2.0)
        assert (col, row) == (0, 0)
        col, row = m.world_to_map(1.99, 1.99)
        assert (col, row) == (3, 3)
