#!/usr/bin/env bash
# Run `ros2 <args>` under the activated pixi env + workspace install.
# Useful one-liners:
#   pixi run ros2 topic list
#   pixi run ros2 topic echo /move_base_flex/astar/plan
#   pixi run ros2 action send_goal /move_base_flex/move_base ...
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
# shellcheck source=./activate.sh
source "${SCRIPT_DIR}/activate.sh"

WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
if [ -f "${WORKSPACE_ROOT}/install/setup.bash" ]; then
    set +u
    # shellcheck disable=SC1091
    source "${WORKSPACE_ROOT}/install/setup.bash"
    set -u
fi

export DYLD_LIBRARY_PATH="${WORKSPACE_ROOT}/install_nav_core/lib:${DYLD_LIBRARY_PATH:-}"
export LD_LIBRARY_PATH="${WORKSPACE_ROOT}/install_nav_core/lib:${LD_LIBRARY_PATH:-}"

exec ros2 "$@"
