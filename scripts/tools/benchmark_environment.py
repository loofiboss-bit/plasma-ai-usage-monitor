"""Create a writable profile exclusively for an isolated benchmark process."""

from __future__ import annotations

import os
from pathlib import Path


def isolated_environment(root: Path) -> dict[str, str]:
    environment = os.environ.copy()
    for key in (
        "HOME",
        "XDG_DATA_HOME",
        "XDG_CONFIG_HOME",
        "XDG_CACHE_HOME",
        "XDG_STATE_HOME",
        "XDG_RUNTIME_DIR",
    ):
        directory = root / key
        directory.mkdir(mode=0o700, parents=True)
        environment[key] = str(directory)
    environment.update(
        DISPLAY="",
        WAYLAND_DISPLAY="",
        QT_QPA_PLATFORM="offscreen",
        XDG_DATA_DIRS="/usr/local/share:/usr/share",
        XDG_CONFIG_DIRS="/etc/xdg",
        PLASMA_AI_MONITOR_DEMO="0",
    )
    return environment
