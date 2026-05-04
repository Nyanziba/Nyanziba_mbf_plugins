"""pytest discovery shim for the in-tree texnitis_mbf_sim package.

When the repo is checked out without a colcon install (e.g. on plain
GitHub Actions runners that only run `pytest`) the package directory
needs to be on `sys.path` so the kinematics module can be imported.
ament_cmake_pytest already handles this when the package is installed,
so this file is a no-op in that case.
"""

from __future__ import annotations

import sys
from pathlib import Path

PACKAGE_ROOT = Path(__file__).resolve().parents[1]
if str(PACKAGE_ROOT) not in sys.path:
    sys.path.insert(0, str(PACKAGE_ROOT))
