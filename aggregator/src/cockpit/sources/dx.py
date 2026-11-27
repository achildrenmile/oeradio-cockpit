"""DX cluster spots from a Telnet stream, deduplicated and bundled."""

import asyncio
import re
from collections.abc import Awaitable, Callable
from datetime import UTC, datetime, timedelta

import structlog

from ..backoff import backoff_delay
from ..bands import band_for
from ..dxcc import DxccTable
from ..models import Dx, DxSpot, now
from ..models.dx import MAX_SPOTS
from ..modes import guess_mode
from .base import Source

DEDUPE_SECONDS = 5 * 60
SPOT_TTL = 60 * 60
IDLE_TIMEOUT = 10 * 60
LOGIN_TIMEOUT = 30
RECONNECT_BASE = 5.0
RECONNECT_CAP = 600.0

_SPOT = re.compile(
    r"^DX de\s+(?P<de>[A-Z0-9/#-]+):\s*(?P<f>\d+(?:\.\d+)?)\s+(?P<dx>[A-Z0-9/]+)\s+"
    r"(?P<cmt>.*?)\s*\b(?P<hhmm>(?:[01]\d|2[0-3])[0-5]\d)Z"
)
_TELNET_NEGOTIATION = re.compile(rb"\xff[\xfb-\xfe].|\xff[\xf0-\xfa]", re.DOTALL)
_LOGIN_PROMPT = re.compile(rb"(login|call(sign)?)\s*:", re.IGNORECASE)

Connect = Callable[[str, int], Awaitable[tuple[asyncio.StreamReader, asyncio.StreamWriter]]]

log = structlog.get_logger(__name__)


def decode_line(raw: bytes) -> str:
    raw = _TELNET_NEGOTIATION.sub(b"", raw).replace(b"\x07", b"")
    try:
        return raw.decode("utf-8").strip()
    except UnicodeDecodeError:
        return raw.decode("latin-1").strip()


def _spot_time(hhmm: str, reference: int) -> int:
    """The cluster only sends HHMM (UTC); pick the matching day."""
    seen = datetime.fromtimestamp(reference, UTC)
    spotted = seen.replace(hour=int(hhmm[:2]), minute=int(hhmm[2:]), second=0, microsecond=0)
    if spotted - seen > timedelta(minutes=10):
        spotted -= timedelta(days=1)
    return int(spotted.timestamp())


def parse_spot(line: str, dxcc: DxccTable, reference: int) -> DxSpot | None:
    match = _SPOT.match(line)
    if not match:
        return None
    khz = float(match["f"])
    band = band_for(khz)
    if band is None:
        return None
    comment = " ".join(match["cmt"].split())
    return DxSpot(
        t=_spot_time(match["hhmm"], reference),
        dx=match["dx"],
        f=round(khz),
        b=band,
        m=guess_mode(khz, band, comment),
        de=match["de"],
        cmt=comment or None,
        dxcc=dxcc.lookup(match["dx"]),
    )


class SpotBuffer:
    """Newest first; one entry per DX station and band within the dedupe window."""

    def __init__(self) -> None:
        self._spots: dict[tuple[str, str], DxSpot] = {}

    def add(self, spot: DxSpot) -> None:
        key = (spot.dx, spot.b)
        known = self._spots.get(key)
        if known and abs(spot.t - known.t) < DEDUPE_SECONDS:
            return
        self._spots[key] = spot

    def snapshot(self, reference: int) -> list[DxSpot]:
        self._spots = {k: s for k, s in self._spots.items() if reference - s.t <= SPOT_TTL}
        return sorted(self._spots.values(), key=lambda s: s.t, reverse=True)[:MAX_SPOTS]


class DxSource(Source):
    name = "dx"
    backoff = False

    def __init__(
        self,
        host: str,
        port: int,
        callsign: str,
        interval: float,
        dxcc: DxccTable,
        connect: Connect = asyncio.open_connection,
    ) -> None:
        self.interval = interval
        self.buffer = SpotBuffer()
        self.connected = False
        self._host = host
        self._port = port
        self._callsign = callsign
        self._dxcc = dxcc
        self._connect = connect

    async def fetch(self) -> Dx:
        if not self.connected:
            raise ConnectionError("not connected to the DX cluster")
        spots = self.buffer.snapshot(now())
        if not spots:
            # Do not overwrite the retained message right after a restart.
            raise LookupError("no spots received yet")
        return Dx(ts=now(), src_ts=spots[0].t, spots=spots)

    async def listen(self) -> None:
        failures = 0
        while True:
            try:
                await self.session()
            except (OSError, TimeoutError, asyncio.IncompleteReadError, ValueError) as error:
                # A session that got as far as logging in starts the backoff over.
                failures = 1 if self.connected else failures + 1
                delay = backoff_delay(failures, RECONNECT_BASE, RECONNECT_CAP)
                log.warning("dx_disconnected", error=repr(error), retry_in=delay)
                self.connected = False
                await asyncio.sleep(delay)

    async def session(self) -> None:
        """One connection: log in, then read until it breaks."""
        self.connected = False
        async with asyncio.timeout(LOGIN_TIMEOUT):
            reader, writer = await self._connect(self._host, self._port)
        try:
            async with asyncio.timeout(LOGIN_TIMEOUT):
                await self._login(reader, writer)
            self.connected = True
            log.info("dx_connected", host=self._host)
            while True:
                async with asyncio.timeout(IDLE_TIMEOUT):
                    raw = await reader.readline()
                if not raw:
                    raise ConnectionError("connection closed by the cluster")
                if spot := parse_spot(decode_line(raw), self._dxcc, now()):
                    self.buffer.add(spot)
        finally:
            writer.close()

    async def _login(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        # The prompt has no line ending, so read chunks.
        seen = b""
        while not _LOGIN_PROMPT.search(seen):
            chunk = await reader.read(1024)
            if not chunk:
                raise ConnectionError("connection closed before the login prompt")
            seen = (seen + chunk)[-4096:]
        writer.write(self._callsign.encode("ascii") + b"\r\n")
        await writer.drain()
