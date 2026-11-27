"""Active POTA spots, Europe only, Carinthia first."""

from collections.abc import Sequence
from datetime import UTC, datetime
from typing import Any

import httpx
import structlog

from ..bands import band_for
from ..models import Pota, PotaSpot, now
from .base import Source
from .http import get

CARINTHIA = "AT-KA"

log = structlog.get_logger(__name__)


def _parse_spot(raw: dict[str, Any]) -> PotaSpot | None:
    try:
        khz = float(raw["frequency"])
        band = band_for(khz)
        if band is None:
            return None
        spotted = datetime.fromisoformat(raw["spotTime"])
        if spotted.tzinfo is None:
            spotted = spotted.replace(tzinfo=UTC)
        return PotaSpot(
            id=raw["spotId"],
            t=int(spotted.timestamp()),
            act=raw["activator"],
            ref=raw["reference"],
            park=raw.get("name") or raw.get("parkName") or None,
            loc=raw.get("locationDesc") or None,
            f=round(khz),
            b=band,
            m=(raw.get("mode") or "").upper() or None,
            sp=raw["spotter"],
            cmt=(raw.get("comments") or "").strip() or None,
        )
    except (KeyError, TypeError, ValueError, OverflowError):
        return None


def _rank(spot: PotaSpot, neighbours: Sequence[str]) -> int:
    program = spot.ref.split("-")[0]
    if CARINTHIA in (spot.loc or "").split(","):
        return 0
    if program == "AT":
        return 1
    if program in neighbours:
        return 2 + list(neighbours).index(program)
    return 2 + len(neighbours)


def parse_pota(data: object, europe: Sequence[str], neighbours: Sequence[str]) -> Pota:
    if not isinstance(data, list):
        raise ValueError("expected a list of spots")
    spots, broken = [], 0
    for raw in data:
        if not isinstance(raw, dict) or raw.get("invalid"):
            continue
        if str(raw.get("reference", "")).split("-")[0] not in europe:
            continue
        if spot := _parse_spot(raw):
            spots.append(spot)
        else:
            broken += 1
    if broken:
        log.debug("pota_spots_skipped", count=broken)
    spots.sort(key=lambda s: (_rank(s, neighbours), -s.t))
    return Pota(ts=now(), src_ts=max((s.t for s in spots), default=None), spots=spots)


class PotaSource(Source):
    name = "pota"

    def __init__(
        self,
        client: httpx.AsyncClient,
        url: str,
        interval: float,
        europe: Sequence[str],
        neighbours: Sequence[str],
    ) -> None:
        self.interval = interval
        self._client = client
        self._url = url
        self._europe = europe
        self._neighbours = neighbours

    async def fetch(self) -> Pota:
        response = await get(self._client, self._url)
        return parse_pota(response.json(), self._europe, self._neighbours)
