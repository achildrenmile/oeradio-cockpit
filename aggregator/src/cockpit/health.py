"""Liveness heartbeat: a file touched by the status loop."""

import time
from pathlib import Path

MAX_AGE_INTERVALS = 3


def beat(path: Path) -> None:
    path.touch()


def is_alive(path: Path, interval: float) -> bool:
    try:
        age = time.time() - path.stat().st_mtime
    except OSError:
        return False
    return age <= MAX_AGE_INTERVALS * interval
