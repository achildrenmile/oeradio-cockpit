"""Source interface and the loop that runs one source."""

import asyncio
import time
from abc import ABC, abstractmethod
from collections.abc import Awaitable, Callable

import structlog

from ..backoff import backoff_delay
from ..models import Message

BACKOFF_CAP = 3600.0
FETCH_TIMEOUT = 60.0

log = structlog.get_logger(__name__)


class Source(ABC):
    """A data source. ``name`` is also the topic it publishes to."""

    name: str
    interval: float
    # Sources that only snapshot an internal buffer set this to False: backing off
    # would delay recovery without sparing any external service.
    backoff: bool = True

    @abstractmethod
    async def fetch(self) -> Message: ...

    async def listen(self) -> None:  # noqa: B027 - optional hook
        """Long running task of streaming sources; polling sources have none."""


class SourceRunner:
    """Fetches and publishes one source, strictly sequentially."""

    def __init__(
        self,
        source: Source,
        publish: Callable[[str, Message], Awaitable[None]],
        clock: Callable[[], float] = time.monotonic,
        sleep: Callable[[float], Awaitable[None]] = asyncio.sleep,
    ) -> None:
        self.source = source
        self.failures = 0
        self.last_success: float | None = None
        self._publish = publish
        self._clock = clock
        self._sleep = sleep

    async def run_once(self) -> float:
        """Run one cycle and return the delay before the next one."""
        try:
            async with asyncio.timeout(FETCH_TIMEOUT):
                message = await self.source.fetch()
            await self._publish(self.source.name, message)
        except Exception as error:
            self.failures += 1
            # Never retry faster than the regular interval, to spare external services.
            cap = BACKOFF_CAP if self.source.backoff else self.source.interval
            delay = backoff_delay(self.failures, self.source.interval, cap)
            log.warning(
                "source_failed",
                source=self.source.name,
                error=repr(error),
                failures=self.failures,
                retry_in=delay,
            )
            return delay
        self.failures = 0
        self.last_success = self._clock()
        return self.source.interval

    def healthy(self) -> bool:
        """Last attempt succeeded and the data is at most three intervals old."""
        if self.failures or self.last_success is None:
            return False
        return self._clock() - self.last_success <= 3 * self.source.interval

    async def run(self) -> None:
        while True:
            await self._sleep(await self.run_once())
