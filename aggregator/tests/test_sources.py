import pytest

from cockpit.config import DATA_TOPICS, Settings
from cockpit.models import TOPIC_MODELS, Message, News
from cockpit.models.base import WARN_BYTES
from cockpit.sources import DemoSource, Source, SourceRunner, build_sources
from cockpit.status import build_status


class Clock:
    now = 1000.0

    def __call__(self) -> float:
        return self.now


class Flaky(Source):
    name = "news"
    interval = 100.0
    fail = False

    async def fetch(self) -> Message:
        if self.fail:
            raise RuntimeError("boom")
        return News(ts=1, items=[])


async def discard(name: str, message: Message) -> None:
    return None


async def test_backoff_grows_from_interval_is_capped_and_resets() -> None:
    source = Flaky()
    runner = SourceRunner(source, discard)
    source.fail = True
    delays = [await runner.run_once() for _ in range(8)]
    assert delays == [100, 200, 400, 800, 1600, 3200, 3600, 3600]
    source.fail = False
    assert await runner.run_once() == 100
    assert runner.failures == 0


async def test_publish_error_counts_as_failure() -> None:
    async def broken(name: str, message: Message) -> None:
        raise ValueError("too large")

    runner = SourceRunner(Flaky(), broken)
    await runner.run_once()
    assert runner.failures == 1 and runner.last_success is None


async def test_status_ok_stale_and_failed() -> None:
    clock = Clock()
    source = Flaky()
    runner = SourceRunner(source, discard, clock=clock)

    def news_ok() -> bool:
        status = build_status([runner], None)
        assert status.ok is not None and set(status.ok) == set(DATA_TOPICS)
        return status.ok["news"]

    assert not news_ok()  # never succeeded
    await runner.run_once()
    assert news_ok()
    clock.now += 300
    assert news_ok()  # exactly three intervals old
    clock.now += 1
    assert not news_ok()  # stale
    await runner.run_once()
    source.fail = True
    await runner.run_once()
    assert not news_ok()  # last attempt failed


def test_motd_is_optional() -> None:
    assert build_status([], "").motd is None
    assert build_status([], "Servus").motd == "Servus"


@pytest.mark.parametrize("topic", DATA_TOPICS)
async def test_demo_data_matches_model_and_size_target(topic: str) -> None:
    message = await DemoSource(topic, 1).fetch()
    assert isinstance(message, TOPIC_MODELS[topic])
    assert len(message.encoded()) < WARN_BYTES


def test_sources_from_environment(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("COCKPIT_SOURCES", " demo ,")
    monkeypatch.setenv("COCKPIT_INTERVAL_DX", "5")
    sources = build_sources(Settings())
    assert [s.name for s in sources] == list(DATA_TOPICS)
    assert {s.name: s.interval for s in sources}["dx"] == 5
    monkeypatch.setenv("COCKPIT_SOURCES", "")
    assert build_sources(Settings()) == []
    monkeypatch.setenv("COCKPIT_SOURCES", "nope")
    with pytest.raises(ValueError, match="nope"):
        build_sources(Settings())
