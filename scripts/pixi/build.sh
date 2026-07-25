#!/usr/bin/env bash
# Two-stage build: nav_core (plain cmake) then the rest of the workspace
# (colcon). CMake's incremental build keeps warm builds fast while ensuring
# changed nav_core headers and archives are always reinstalled for colcon.
#
# Sources scripts/pixi/activate.sh first so CC/CXX/SDKROOT and the conda
# activate.d hooks are in place before cmake / colcon run, and rewrites
# Python shebangs at the end so rclpy nodes survive macOS SIP.
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
# shellcheck source=./activate.sh
source "${SCRIPT_DIR}/activate.sh"

cmake -S src/texnitis_nav_core -B build_nav_core \
    -DCMAKE_BUILD_TYPE=Release \
    -DNAV_CORE_BUILD_PYTHON=OFF \
    -DNAV_CORE_BUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX="$(pwd)/install_nav_core"
cmake --build build_nav_core -j
cmake --install build_nav_core

export texnitis_nav_core_DIR="$(pwd)/install_nav_core/lib/cmake/texnitis_nav_core"
export CMAKE_PREFIX_PATH="$(pwd)/install_nav_core:${CMAKE_PREFIX_PATH:-}"

# Plain (non-symlink) install on macOS so fix_shebangs operates on copies
# in install/<pkg>/lib/ instead of editing the source tree through symlinks.
colcon build \
    --packages-up-to texnitis_mbf_sim mbf_simple_nav \
    --cmake-args -DCMAKE_BUILD_TYPE=Release \
    "-DPython3_EXECUTABLE=${Python3_EXECUTABLE}" \
    "-DPython3_ROOT_DIR=${Python3_ROOT_DIR}" \
    "-Dtexnitis_nav_core_DIR=$texnitis_nav_core_DIR"

# colcon writes shebangs back to /usr/bin/env python3; pin them to the env's
# interpreter so SIP does not strip DYLD_LIBRARY_PATH at exec().
fix_shebangs
