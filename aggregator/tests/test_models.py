import json

import pytest
from pydantic import ValidationError

from cockpit import schemas
from cockpit.models import (
    TOPIC_MODELS,
    Dx,
    DxSpot,
    Mesh,
    MeshMessage,
    News,
    NewsItem,
    Pota,
    PotaSpot,
    Status,
    truncate,
)
from cockpit.models.base import MAX_BYTES


def test_truncate_keeps_short_text_and_marks_cut() -> None:
    assert truncate("Grüße", 5) == "Grüße"
    assert truncate("abcdefgh", 5) == "abcd…"


def test_text_limits_are_applied() -> None:
    assert len(MeshMessage(t=0, sender="x", ch="Public", txt="ä" * 200).txt) == 80
    assert len(NewsItem(t=0, title="x" * 200, url="https://oeradio.at/").title) == 90
    status = Status(ts=0, agg="0.1.0", ok={}, motd="m" * 200)
    assert status.motd is not None and len(status.motd) == 80


def test_lists_are_capped_keeping_the_first_items() -> None:
    items = [NewsItem(t=i, title="x", url="u") for i in range(20)]
    news = News(ts=0, items=items)
    assert [item.t for item in news.items] == list(range(8))


@pytest.mark.parametrize("model", [Pota, Dx])
def test_max_cannot_exceed_contract(model: type[Pota] | type[Dx]) -> None:
    with pytest.raises(ValidationError):
        model(ts=0, max=41, spots=[])


def test_oversized_spot_lists_drop_the_tail_to_fit() -> None:
    pota = Pota(
        ts=1764268800,
        spots=[
            PotaSpot(
                id=10**8,
                t=1764268800,
                act="OE8XXX/P",
                ref="AT-0000",
                park="ü" * 99,
                loc="AT-KA",
                f=14285,
                b="20m",
                m="SSB",
                sp="OE8XXX",
                cmt="c" * 99,
            )
        ]
        * 100,
    )
    dx = Dx(
        ts=1764268800,
        spots=[
            DxSpot(
                t=1764268800,
                dx="VP8/OE8XXX",
                f=14025,
                b="20m",
                m="CW",
                de="OE8XXX",
                cmt="c" * 99,
                dxcc="South Sandwich Islands",
            )
        ]
        * 100,
    )
    for message in (pota, dx):
        assert len(message.encoded()) <= MAX_BYTES
        assert 25 < len(message.spots) <= 40


def test_wire_format_is_compact_and_uses_from() -> None:
    mesh = Mesh(
        ts=1,
        nodes_online=1,
        repeaters_online=0,
        recent=[MeshMessage.model_validate({"t": 1, "from": "N", "ch": "Public", "txt": "hi"})],
        top_repeaters=[],
    )
    payload = mesh.encoded().decode()
    assert '"from":"N"' in payload
    assert "src_ts" not in payload
    assert " " not in payload


def test_status_requires_agg_and_ok_unless_offline() -> None:
    assert json.loads(Status(ts=5, offline=True).encoded()) == {"v": 1, "ts": 5, "offline": True}
    with pytest.raises(ValidationError):
        Status(ts=5)


@pytest.mark.parametrize("topic", TOPIC_MODELS)
def test_checked_in_schema_is_current(topic: str) -> None:
    path = schemas.DEFAULT_DIR / f"{topic}.schema.json"
    assert json.loads(path.read_text(encoding="utf-8")) == schemas.schema_for(topic)
    assert {"v", "ts"} <= set(schemas.schema_for(topic)["required"])
