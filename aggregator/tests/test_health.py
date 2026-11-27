import asyncio
import os
from pathlib import Path

import pytest

from cockpit import __main__, health
from cockpit.models import Message
from cockpit.status import run_status


async def test_status_loop_beats(tmp_path: Path) -> None:
    path = tmp_path / "beat"

    async def publish(name: str, message: Message) -> None:
        return None

    task = asyncio.create_task(run_status([], publish, 60, None, path))
    await asyncio.sleep(0.01)
    task.cancel()
    assert health.is_alive(path, 60)


def test_missing_or_old_file_is_not_alive(tmp_path: Path) -> None:
    path = tmp_path / "beat"
    assert not health.is_alive(path, 30)
    health.beat(path)
    os.utime(path, (0, 0))
    assert not health.is_alive(path, 30)


@pytest.mark.parametrize(("exists", "code"), [(True, 0), (False, 1)])
def test_healthcheck_command(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path, exists: bool, code: int
) -> None:
    path = tmp_path / "beat"
    if exists:
        path.touch()
    monkeypatch.setenv("COCKPIT_HEALTH_FILE", str(path))
    with pytest.raises(SystemExit) as exit_info:
        __main__.main(["healthcheck"])
    assert exit_info.value.code == code
