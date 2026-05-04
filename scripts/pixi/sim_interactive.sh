#!/usr/bin/env bash
# Like sim_e2e.sh but leaves the launch alive so an external client
# (rviz2, waypoint_sender, ros2 action send_goal) drives the goals.
set -eo pipefail
# shellcheck disable=SC1091
source install/setup.bash
set -u
export LD_LIBRARY_PATH="$(pwd)/install_nav_core/lib:${LD_LIBRARY_PATH:-}"
ros2 launch texnitis_mbf_sim texnitis_mbf_sim_demo.launch.py e2e_check:=false
