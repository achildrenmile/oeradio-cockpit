import asyncio
import hashlib
import hmac
import json
from collections.abc import AsyncIterator
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import aiomqtt
import pytest
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

from cockpit.models import now
from cockpit.sources import mesh as mesh_module
from cockpit.sources.mesh import MeshSource, MeshState
from cockpit.sources.meshcore import (
    PUBLIC_KEY,
    Channel,
    ChannelText,
    NodeSeen,
    PacketCaptureAdapter,
)

PACKETS = json.loads((Path(__file__).parent / "fixtures/mesh/packets.json").read_text("utf-8"))


def group_packet(key: bytes, text: str, hops: int = 0, transport: bool = False) -> str:
    plain = bytes(4) + b"\x00" + text.encode() + b"\x00"
    plain += bytes(-len(plain) % 16)
    encryptor = Cipher(algorithms.AES(key), modes.ECB()).encryptor()
    ciphertext = encryptor.update(plain) + encryptor.finalize()
    mac = hmac.new(key + bytes(16), ciphertext, hashlib.sha256).digest()[:2]
    header = bytes([(0x05 << 2) | (0x03 if transport else 0x01)])
    middle = b"\xaa\xbb\xcc\xdd" if transport else b""
    path = bytes([0x40 | hops]) + bytes(range(1, hops * 2 + 1))
    channel_hash = hashlib.sha256(key).digest()[:1]
    return (header + middle + path + channel_hash + mac + ciphertext).hex()


def text_payload(key: bytes, text: str, packet: str = "AB", **kw: Any) -> bytes:
    raw = group_packet(key, text, **kw)
    data = {"type": "PACKET", "packet_type": "5", "hash": packet, "raw": raw, "decoded": {}}
    return json.dumps(data).encode()


def adapter(*names: str) -> PacketCaptureAdapter:
    return PacketCaptureAdapter([Channel.from_name(n) for n in names or ("public",)])


def test_adverts_from_fixture() -> None:
    events = [adapter().parse(json.dumps(p).encode()) for p in PACKETS]
    assert events[0] == NodeSeen("a1" * 32, "Test-Repeater", True)
    assert events[1] == NodeSeen("b2" * 32, "Test-Handgerät", False)
    assert events[2:] == [None, None]
    unverified = {**PACKETS[0], "decoded": {**PACKETS[0]["decoded"], "advert_parse_ok": False}}
    assert adapter().parse(json.dumps(unverified).encode()) is None


@pytest.mark.parametrize(("hops", "transport"), [(0, False), (3, False), (2, True)])
def test_public_channel_decrypts(hops: int, transport: bool) -> None:
    payload = text_payload(PUBLIC_KEY, "Anna: Servus", hops=hops, transport=transport)
    assert adapter().parse(payload) == ChannelText("ab", "Anna", "Public", "Servus")


def test_only_whitelisted_channels_are_read() -> None:
    key = Channel.from_name("#at-ktn").key
    assert adapter("public").parse(text_payload(key, "x: geheim")) is None
    event = adapter("#at-ktn").parse(text_payload(key, "ohne Absender"))
    assert isinstance(event, ChannelText) and (event.sender, event.channel) == ("?", "#at-ktn")
    with pytest.raises(ValueError):
        Channel.from_name("intern")


def test_state_counts_window_and_dedupes() -> None:
    state = MeshState(window=100)
    state.add(NodeSeen("a", "Rep", True), 1000)
    state.add(NodeSeen("b", "Handy", False), 1090)
    state.add(ChannelText("p1", "Anna", "Public", "ü" * 200), 1090)
    state.add(ChannelText("p1", "Anna", "Public", "again"), 1091)
    mesh = state.snapshot(1095)
    assert (mesh.nodes_online, mesh.repeaters_online, len(mesh.recent)) == (2, 1, 1)
    assert len(mesh.recent[0].txt) == 80
    assert mesh.top_repeaters[0].name == "Rep"
    later = state.snapshot(1150)
    assert (later.nodes_online, later.repeaters_online) == (1, 0)


class FakeClient:
    def __init__(self, messages: list[SimpleNamespace]) -> None:
        self._messages = messages
        self.topics: list[str] = []

    async def __aenter__(self) -> "FakeClient":
        return self

    async def __aexit__(self, *args: object) -> None:
        return None

    async def subscribe(self, topic: str) -> None:
        self.topics.append(topic)

    @property
    def messages(self) -> AsyncIterator[SimpleNamespace]:
        async def stream() -> AsyncIterator[SimpleNamespace]:
            for message in self._messages:
                yield message
            await asyncio.Event().wait()

        return stream()


async def test_source_consumes_stream(monkeypatch: pytest.MonkeyPatch) -> None:
    advert = json.dumps(PACKETS[0]).encode()
    old = json.dumps(PACKETS[1]).encode()
    client = FakeClient(
        [SimpleNamespace(retain=True, payload=old), SimpleNamespace(retain=False, payload=advert)]
    )
    source = MeshSource(lambda: client, "meshcore/+/+/packets", adapter(), 30, 3600)
    with pytest.raises(ConnectionError):
        await source.fetch()
    task = asyncio.create_task(source.listen())
    async with asyncio.timeout(2):
        while source.last_packet is None:  # noqa: ASYNC110
            await asyncio.sleep(0)
    assert client.topics == ["meshcore/+/+/packets"]
    assert (await source.fetch()).nodes_online == 1
    source.last_packet = now() - 3600
    with pytest.raises(TimeoutError):
        await source.fetch()
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task


def test_unknown_format_is_logged_once(monkeypatch: pytest.MonkeyPatch) -> None:
    errors: list[str] = []
    monkeypatch.setattr(
        mesh_module.log, "error", lambda event, **kw: errors.append(event), raising=False
    )
    source = MeshSource(lambda: FakeClient([]), "t", adapter(), 30, 3600)
    source.handle(b"not json")
    source.handle(b"[1]")
    assert errors == ["mesh_unknown_format"] and source.last_packet is None


async def test_reconnects_after_broker_error(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(mesh_module, "RECONNECT_BASE", 0.001)
    attempts = 0

    def factory() -> FakeClient:
        nonlocal attempts
        attempts += 1
        if attempts == 1:
            raise aiomqtt.MqttError("refused")
        return FakeClient([])

    source = MeshSource(factory, "t", adapter(), 30, 3600)
    task = asyncio.create_task(source.listen())
    async with asyncio.timeout(2):
        while not source.connected:  # noqa: ASYNC110
            await asyncio.sleep(0.001)
    assert attempts == 2
    with pytest.raises(LookupError):
        await source.fetch()
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task
