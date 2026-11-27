"""Periodic status message."""

import asyncio
from collections.abc import Awaitable, Callable, Sequence
from pathlib import Path

from . import __version__, health
from .config import DATA_TOPICS
from .models import Message, Status, now
from .sources import SourceRunner


def build_status(runners: Sequence[SourceRunner], motd: str | None) -> Status:
    """Every data topic is listed; topics without an enabled source are not ok."""
    ok = dict.fromkeys(DATA_TOPICS, False)
    ok.update({runner.source.name: runner.healthy() for runner in runners})
    return Status(ts=now(), agg=__version__, ok=ok, motd=motd or None)


async def run_status(
    runners: Sequence[SourceRunner],
    publish: Callable[[str, Message], Awaitable[None]],
    interval: float,
    motd: str | None,
    health_file: Path | None = None,
) -> None:
    while True:
        await publish("status", build_status(runners, motd))
        if health_file:
            health.beat(health_file)
        await asyncio.sleep(interval)
