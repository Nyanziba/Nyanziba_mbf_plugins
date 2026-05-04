#!/usr/bin/env bash
# Two-stage build: nav_core (plain cmake) then the rest of the workspace
# (colcon). Re-using install_nav_core when present keeps warm builds fast.
set -euo pipefail

if [ ! -f install_nav_core/lib/libtexnitis_nav_core.a ]; then
    cmake -S src/texnitis_nav_core -B build_nav_core \
        -DCMAKE_BUILD_TYPE=Release \
        -DNAV_CORE_WITH_MPPI=OFF \
        -DNAV_CORE_BUILD_PYTHON=OFF \
        -DNAV_CORE_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX="$(pwd)/install_nav_core"
    cmake --build build_nav_core -j
    cmake --install build_nav_core
else
    echo "nav_core already installed; skipping"
fi

export texnitis_nav_core_DIR="$(pwd)/install_nav_core/lib/cmake/texnitis_nav_core"
export CMAKE_PREFIX_PATH="$(pwd)/install_nav_core:${CMAKE_PREFIX_PATH:-}"

# CMake's FindPython3 sometimes picks the system /opt/homebrew Python
# over the conda env one. Pin to the pixi env's interpreter so
# rosidl_generator_py finds matching numpy headers.
PYTHON3_EXEC="${CONDA_PREFIX:-$PIXI_PROJECT_ROOT/.pixi/envs/default}/bin/python3"

colcon build \
    --packages-up-to texnitis_mbf_sim mbf_simple_nav \
    --cmake-args -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_MPPI_CONTROLLER=OFF \
    "-DPython3_EXECUTABLE=$PYTHON3_EXEC" \
    "-DPython3_ROOT_DIR=$(dirname $(dirname $PYTHON3_EXEC))" \
    "-Dtexnitis_nav_core_DIR=$texnitis_nav_core_DIR"
