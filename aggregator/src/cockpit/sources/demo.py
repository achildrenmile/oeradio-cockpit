"""Plausible fake data for all data topics."""

import random

from ..models import (
    BandCondition,
    Dx,
    DxSpot,
    Mesh,
    MeshMessage,
    Message,
    News,
    NewsItem,
    Pota,
    PotaSpot,
    Repeater,
    Solar,
    Vhf,
    now,
)
from ..models.solar import Condition
from .base import Source

_FREQUENCIES = [
    (3573, "80m", "FT8"),
    (7032, "40m", "CW"),
    (14285, "20m", "SSB"),
    (18100, "17m", "FT8"),
    (21030, "15m", "CW"),
]
_PARKS = [("AT-0001", "Demo-Park Kärnten", "AT-KA"), ("DE-0001", "Demo-Park Bayern", "DE-BY")]
# Obviously fictitious call signs, so demo data is never mistaken for real activity.
_CALLS = ["N0CALL", "N0CALL/P", "N0CALL/M"]
_CONDITIONS: list[Condition] = ["good", "fair", "poor"]


def _solar() -> Solar:
    bands = [
        BandCondition(b=b, day=random.choice(_CONDITIONS), night=random.choice(_CONDITIONS))
        for b in ("80m-40m", "30m-20m", "17m-15m", "12m-10m")
    ]
    return Solar(
        ts=now(),
        src_ts=now() - 600,
        sfi=random.randint(70, 220),
        ssn=random.randint(0, 200),
        a=random.randint(0, 30),
        k=random.randint(0, 6),
        xray=f"B{random.uniform(1, 9):.1f}",
        wind=random.randint(300, 600),
        bands=bands,
        vhf=Vhf(aurora=False, es_eu="closed"),
    )


def _pota() -> Pota:
    spots = []
    for index in range(8):
        f, band, mode = random.choice(_FREQUENCIES)
        ref, park, loc = random.choice(_PARKS)
        spots.append(
            PotaSpot(
                id=index + 1,
                t=now() - index * 60,
                act=random.choice(_CALLS),
                ref=ref,
                park=park,
                loc=loc,
                f=f,
                b=band,
                m=mode,
                sp="N0CALL",
                cmt="Demo",
            )
        )
    return Pota(ts=now(), spots=spots)


def _dx() -> Dx:
    spots = []
    for index in range(10):
        f, band, mode = random.choice(_FREQUENCIES)
        spots.append(
            DxSpot(
                t=now() - index * 20,
                dx=random.choice(_CALLS),
                f=f,
                b=band,
                m=mode,
                de="N0CALL",
                cmt="Demo",
                dxcc="Demoland",
            )
        )
    return Dx(ts=now(), spots=spots)


def _mesh() -> Mesh:
    message = MeshMessage(t=now() - 90, sender="Demo-Node", ch="Public", txt="Grüße vom Demo-Netz")
    return Mesh(
        ts=now(),
        nodes_online=random.randint(20, 50),
        repeaters_online=random.randint(5, 12),
        recent=[message],
        top_repeaters=[Repeater(name="Demo-Repeater", seen=now() - 30)],
    )


def _news() -> News:
    item = NewsItem(
        t=now() - 86400, title="Demo: Neuigkeiten von oeradio.at", url="https://oeradio.at/"
    )
    return News(ts=now(), items=[item])


_BUILDERS = {"solar": _solar, "pota": _pota, "dx": _dx, "mesh": _mesh, "news": _news}


class DemoSource(Source):
    def __init__(self, name: str, interval: float) -> None:
        if name not in _BUILDERS:
            raise ValueError(f"no demo data for {name!r}")
        self.name = name
        self.interval = interval

    async def fetch(self) -> Message:
        return _BUILDERS[self.name]()
