import asyncio
from datetime import UTC, datetime
from pathlib import Path

import pytest

from cockpit.bands import band_for
from cockpit.dxcc import default_table
from cockpit.models import DxSpot, now
from cockpit.modes import guess_mode
from cockpit.sources import dx as dx_module
from cockpit.sources.dx import DxSource, SpotBuffer, decode_line, parse_spot

LINES = (Path(__file__).parent / "fixtures/dx/spots.txt").read_bytes().splitlines(keepends=True)
EVENING = int(datetime(2026, 11, 27, 19, 35, tzinfo=UTC).timestamp())


def parse(line: str, reference: int = EVENING) -> DxSpot | None:
    return parse_spot(line, default_table(), reference)


def test_recorded_lines_parse() -> None:
    spots = [parse(decode_line(raw)) for raw in LINES]
    assert all(spots)
    first = spots[0]
    assert first is not None
    assert (first.de, first.f, first.dx, first.b) == ("AF4PX", 21355, "7Q2TB", "15m")
    assert (first.m, first.dxcc, first.cmt) == ("SSB", "Malawi", "CCC 7Q*")
    assert first.t == EVENING - 120
    assert [s.m for s in spots if s] == ["SSB", "FT8", "CW", "SSB", "SSB"]


def test_line_variants() -> None:
    skimmer = parse("DX de W3LPL-#:  14025.0  VP8/OE1ABC   CW 21 dB   1934Z JN66")
    assert skimmer is not None
    assert (skimmer.de, skimmer.m, skimmer.dxcc) == ("W3LPL-#", "CW", "Falkland Islands")
    no_comment = parse("DX de OE5XYZ-12:14000.5 OE8XXX/P 1934Z")
    assert no_comment is not None and no_comment.cmt is None and no_comment.dxcc == "Austria"
    yesterday = parse("DX de A1AA: 14025.0 B1BB test 2359Z", EVENING)
    assert yesterday is not None and yesterday.t < EVENING
    assert parse("DX de A1AA: 9000.0 B1BB broadcast 1900Z") is None
    assert parse("WWV de VE7CC <19Z> : SFI=87") is None
    assert decode_line(b"\xff\xfb\x01DX de X\x07\x07\r\n") == "DX de X"
    assert decode_line(b"Gr\xfc\xdfe\r\n") == "Grüße"


@pytest.mark.parametrize(
    ("khz", "comment", "expected"),
    [
        (14074.5, "", "FT8"),
        (7048.0, "", "FT4"),
        (14020, "", "CW"),
        (14250, "", "SSB"),
        (14090, "", None),
        (10125, "", "CW"),
        (10145, "", None),
        (3700, "", "SSB"),
        (145500, "", None),
        (14250, "rtty test", "RTTY"),
        (14250, "LSB", "SSB"),
    ],
)
def test_mode_guess(khz: float, comment: str, expected: str | None) -> None:
    assert guess_mode(khz, band_for(khz) or "", comment) == expected


def test_dxcc_lookup() -> None:
    table = default_table()
    assert table.lookup("OE8XXX") == "Austria"
    assert table.lookup("DL1ABC/P") == "Fed. Rep. of Germany"
    assert table.lookup("OE1ABC/MM") is None
    assert table.lookup("4U1VIC") == "Austria"  # exact call entry
    assert table.lookup("/P") is None
    assert table.lookup("QQQ") is None


def spot(t: int, dx: str = "B1BB", b: str = "20m") -> DxSpot:
    return DxSpot(t=t, dx=dx, f=14025, b=b, de="A1AA")


def test_dedupe_and_expiry() -> None:
    buffer = SpotBuffer()
    buffer.add(spot(1000))
    buffer.add(spot(1200))  # same station and band within five minutes
    buffer.add(spot(1100, b="40m"))
    assert [s.t for s in buffer.snapshot(1200)] == [1100, 1000]
    buffer.add(spot(1300))
    assert [s.t for s in buffer.snapshot(1300)] == [1300, 1100]
    assert buffer.snapshot(1300 + 3601) == []


class FakeWriter:
    def __init__(self) -> None:
        self.sent = b""
        self.closed = False

    def write(self, data: bytes) -> None:
        self.sent += data

    async def drain(self) -> None:
        return None

    def close(self) -> None:
        self.closed = True


async def test_stream_login_publish_and_reconnect(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(dx_module, "RECONNECT_BASE", 0.001)
    minute = datetime.now(UTC).strftime("%H%MZ")
    readers = [asyncio.StreamReader(), asyncio.StreamReader()]
    writers = [FakeWriter(), FakeWriter()]
    pairs = iter(zip(readers, writers, strict=True))

    async def connect(host: str, port: int):  # type: ignore[no-untyped-def]
        return next(pairs)

    source = DxSource("h", 1, "N0CALL", 15, default_table(), connect)
    with pytest.raises(ConnectionError):
        await source.fetch()
    task = asyncio.create_task(source.listen())

    async def until(condition):  # type: ignore[no-untyped-def]
        async with asyncio.timeout(2):
            while not condition():  # noqa: ASYNC110
                await asyncio.sleep(0.001)

    readers[0].feed_data(b"Welcome\r\nlogin: ")
    await until(lambda: source.connected)
    assert writers[0].sent == b"N0CALL\r\n"
    with pytest.raises(LookupError):
        await source.fetch()
    readers[0].feed_data(f"noise\r\nDX de A1AA: 14025.0 OE8XXX test {minute}\r\n".encode())
    await until(lambda: source.buffer.snapshot(now()))
    assert (await source.fetch()).spots[0].dx == "OE8XXX"

    readers[0].feed_eof()
    readers[1].feed_data(b"Please enter your call: ")
    await until(lambda: writers[1].sent)
    assert writers[0].closed
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task


async def test_connection_closed_before_prompt() -> None:
    reader = asyncio.StreamReader()
    reader.feed_eof()

    async def connect(host: str, port: int):  # type: ignore[no-untyped-def]
        return reader, FakeWriter()

    with pytest.raises(ConnectionError):
        await DxSource("h", 1, "N0CALL", 15, default_table(), connect).session()
