import asyncio
import json
from collections.abc import AsyncIterator
from types import TracebackType
from typing import Self

import aiomqtt
import pytest

from cockpit import publisher as publisher_module
from cockpit.config import Settings
from cockpit.models import News, NewsItem
from cockpit.publisher import PayloadTooLargeError, Publisher

NEWS_TOPIC = "oeradio/cockpit/v1/news"
STATUS_TOPIC = "oeradio/cockpit/v1/status"


class FakeClient:
    def __init__(self) -> None:
        self.sent: list[tuple[str, bytes, int, bool]] = []
        self.drop = asyncio.Event()

    async def __aenter__(self) -> Self:
        return self

    async def __aexit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: TracebackType | None,
    ) -> None:
        return None

    @property
    def messages(self) -> AsyncIterator[None]:
        async def stream() -> AsyncIterator[None]:
            await self.drop.wait()
            raise aiomqtt.MqttError("connection lost")
            yield

        return stream()

    async def publish(self, topic: str, /, payload: bytes, qos: int, retain: bool) -> None:
        self.sent.append((topic, payload, qos, retain))


def news(title: str = "Hallo", url: str = "https://oeradio.at/") -> News:
    return News(ts=1, items=[NewsItem(t=1, title=title, url=url)])


async def until(condition: object) -> None:
    async with asyncio.timeout(2):
        while not condition():  # type: ignore[operator]  # noqa: ASYNC110
            await asyncio.sleep(0)


async def test_publishes_retained_qos1_and_offline_on_shutdown() -> None:
    client = FakeClient()
    wills: list[aiomqtt.Will] = []

    def factory(will: aiomqtt.Will) -> FakeClient:
        wills.append(will)
        return client

    publisher = Publisher(Settings(), factory)
    task = asyncio.create_task(publisher.run())
    await until(lambda: publisher.connected)
    await publisher.publish("news", news())

    topic, payload, qos, retain = client.sent[-1]
    assert (topic, qos, retain) == (NEWS_TOPIC, 1, True)
    assert json.loads(payload)["items"][0]["title"] == "Hallo"
    assert wills[0].topic == STATUS_TOPIC
    assert wills[0].retain and wills[0].qos == 1
    assert json.loads(wills[0].payload)["offline"] is True  # type: ignore[arg-type]

    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task
    assert client.sent[-1][0] == STATUS_TOPIC
    assert json.loads(client.sent[-1][1])["offline"] is True


async def test_reconnects_and_replays_latest(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(publisher_module, "RECONNECT_BASE", 0.001)
    clients = [FakeClient(), FakeClient()]
    remaining = iter(clients)
    publisher = Publisher(Settings(), lambda will: next(remaining))
    await publisher.publish("news", news("alt"))
    await publisher.publish("news", news("neu"))  # while offline, only the latest counts
    task = asyncio.create_task(publisher.run())

    await until(lambda: publisher.connected)
    clients[0].drop.set()
    await until(lambda: clients[1].sent)

    for client in clients:
        assert [json.loads(p)["items"][0]["title"] for _, p, _, _ in client.sent] == ["neu"]
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task


async def test_rejects_payload_over_hard_limit() -> None:
    publisher = Publisher(Settings(), lambda will: FakeClient())
    with pytest.raises(PayloadTooLargeError):
        await publisher.publish("news", news(url="x" * 9000))


async def test_warns_on_large_payload(monkeypatch: pytest.MonkeyPatch) -> None:
    warnings: list[str] = []
    monkeypatch.setattr(
        publisher_module.log, "warning", lambda event, **kw: warnings.append(event), raising=False
    )
    publisher = Publisher(Settings(), lambda will: FakeClient())
    await publisher.publish("news", news(url="x" * 5000))
    assert warnings == ["payload_large"]
