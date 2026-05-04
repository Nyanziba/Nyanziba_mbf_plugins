#!/usr/bin/env bash
# Bring nav_core and move_base_flex into ./src so colcon can see them
# alongside the in-tree texnitis packages. Idempotent: re-runs are a no-op.
set -euo pipefail

mkdir -p src

# Symlink the repo into src/ so colcon discovers our texnitis_mbf_* pkgs.
if [ ! -e src/texnitis_mbf_plugins ]; then
    ln -sf "$(pwd)" src/texnitis_mbf_plugins
fi

if [ ! -d src/move_base_flex ]; then
    vcs import src < third_party/move_base_flex.repos
fi

if [ ! -d src/texnitis_nav_core ]; then
    vcs import src < third_party/texnitis_nav_core.repos
fi

echo "setup ok: $(ls src)"
