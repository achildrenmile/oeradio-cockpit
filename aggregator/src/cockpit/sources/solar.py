"""Solar and band conditions from the N0NBH XML feed on hamqsl.com."""

from datetime import UTC, datetime
from xml.etree.ElementTree import Element

import httpx
from defusedxml.ElementTree import fromstring

from ..models import BandCondition, Solar, Vhf, now
from ..models.solar import Condition
from .base import Source
from .http import get

_CONDITIONS: dict[str, Condition] = {"good": "good", "fair": "fair", "poor": "poor"}
_CLOSED = "band closed"


def _text(data: Element, tag: str) -> str:
    return (data.findtext(tag) or "").strip()


def _number(data: Element, tag: str) -> int | None:
    """Values may be missing, empty or something like "NoRpt"."""
    try:
        return round(float(_text(data, tag)))
    except (ValueError, OverflowError):
        return None


def _updated(value: str) -> int | None:
    try:
        parsed = datetime.strptime(value, "%d %b %Y %H%M GMT")
    except ValueError:
        return None
    return int(parsed.replace(tzinfo=UTC).timestamp())


def parse_solar(xml: bytes) -> Solar:
    data = fromstring(xml).find("solardata")
    if data is None:
        raise ValueError("no <solardata> element")
    sfi = _number(data, "solarflux")
    if sfi is None:
        raise ValueError("no solar flux value")

    bands: dict[str, dict[str, Condition]] = {}
    for band in data.iterfind("calculatedconditions/band"):
        condition = _CONDITIONS.get((band.text or "").strip().lower())
        if condition:
            bands.setdefault(band.get("name", ""), {})[band.get("time", "")] = condition

    aurora, es_eu = False, "closed"
    for item in data.iterfind("calculatedvhfconditions/phenomenon"):
        state = (item.text or "").strip().lower()
        if item.get("name") == "vhf-aurora":
            aurora = state not in ("", _CLOSED)
        elif item.get("name") == "E-Skip" and item.get("location") == "europe":
            es_eu = "closed" if state in ("", _CLOSED) else state

    return Solar(
        ts=now(),
        src_ts=_updated(_text(data, "updated")),
        sfi=sfi,
        ssn=_number(data, "sunspots"),
        a=_number(data, "aindex"),
        k=_number(data, "kindex"),
        xray=_text(data, "xray") or None,
        wind=_number(data, "solarwind"),
        bands=[
            BandCondition(b=name, day=times["day"], night=times["night"])
            for name, times in bands.items()
            if {"day", "night"} <= times.keys()
        ],
        vhf=Vhf(aurora=aurora, es_eu=es_eu),
    )


class SolarSource(Source):
    name = "solar"

    def __init__(self, client: httpx.AsyncClient, url: str, interval: float) -> None:
        self.interval = interval
        self._client = client
        self._url = url

    async def fetch(self) -> Solar:
        return parse_solar((await get(self._client, self._url)).content)
