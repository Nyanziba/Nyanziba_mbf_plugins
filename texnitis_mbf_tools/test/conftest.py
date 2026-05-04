"""Pytest configuration so the package importable without colcon install.

Adds the repository's `texnitis_mbf_tools/texnitis_mbf_tools` directory to
``sys.path`` so the test file can ``from texnitis_mbf_tools.utils import ...``
without first running ``colcon build``.
"""

from __future__ import annotations

import sys
from pathlib import Path

_THIS_DIR = Path(__file__).resolve().parent
_PKG_PARENT = _THIS_DIR.parent      # .../texnitis_mbf_tools/
sys.path.insert(0, str(_PKG_PARENT))
