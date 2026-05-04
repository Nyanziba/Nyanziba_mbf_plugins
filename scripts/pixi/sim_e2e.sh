#!/usr/bin/env bash
# Source the local install and run the one-shot E2E launch headlessly.
# colcon's setup.bash references several optional env vars unguarded
# (COLCON_TRACE, ...). Run sourcing without -u, then re-enable.
set -eo pipefail
# shellcheck disable=SC1091
source install/setup.bash
set -u
export LD_LIBRARY_PATH="$(pwd)/install_nav_core/lib:${LD_LIBRARY_PATH:-}"
ros2 launch texnitis_mbf_sim texnitis_mbf_sim_demo.launch.py \
    e2e_check:=true \
    goal_x:=2.5 goal_y:=0.5 goal_yaw:=0.0 \
    overall_timeout_s:=180.0
