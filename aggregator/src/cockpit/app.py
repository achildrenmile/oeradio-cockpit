"""Wires publisher, sources and status together."""

import asyncio
import contextlib
import signal
from collections.abc import Sequence
from typing import Any

import httpx
import structlog

from . import __version__
from .config import Settings
from .publisher import Publisher
from .sources import Source, SourceRunner, build_sources
from .sources.http import make_client
from .status import run_status

log = structlog.get_logger(__name__)


async def run(settings: Settings, stop: asyncio.Event | None = None) -> None:
    """Run until ``stop`` is set (by default on SIGINT or SIGTERM)."""
    async with make_client() as http:
        await _run(settings, http, stop)


async def _run(settings: Settings, http: httpx.AsyncClient, stop: asyncio.Event | None) -> None:
    publisher = Publisher(settings)
    sources = build_sources(settings, http)
    runners = [SourceRunner(source, publisher.publish) for source in sources]
    if stop is None:
        stop = asyncio.Event()
        loop = asyncio.get_running_loop()
        for signum in (signal.SIGINT, signal.SIGTERM):
            loop.add_signal_handler(signum, stop.set)

    log.info("starting", version=__version__, sources=[r.source.name for r in runners])
    connection = asyncio.create_task(publisher.run(), name="publisher")
    workers = [asyncio.create_task(r.run(), name=r.source.name) for r in runners]
    workers.append(
        asyncio.create_task(
            run_status(runners, publisher.publish, settings.interval_status, settings.motd),
            name="status",
        )
    )
    # Polling sources return from listen() at once, so only streaming ones are watched.
    workers += [
        asyncio.create_task(s.listen(), name=f"{s.name}-listen")
        for s in sources
        if type(s).listen is not Source.listen
    ]
    stopping = asyncio.create_task(stop.wait(), name="stop")
    try:
        done, _ = await asyncio.wait(
            [connection, *workers, stopping], return_when=asyncio.FIRST_COMPLETED
        )
        for task in done:
            task.result()  # A crashed task must end the process, not go unnoticed.
    finally:
        # Workers first, so nothing overwrites the final offline status.
        await _cancel([stopping, *workers])
        await _cancel([connection])
        log.info("stopped")


async def _cancel(tasks: Sequence[asyncio.Task[Any]]) -> None:
    for task in tasks:
        task.cancel()
    for task in tasks:
        with contextlib.suppress(asyncio.CancelledError, Exception):
            await task
