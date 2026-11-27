import json
from pathlib import Path

import httpx
import pytest
import respx

from cockpit.config import EUROPE
from cockpit.sources.http import USER_AGENT, make_client
from cockpit.sources.news import NewsSource, parse_news
from cockpit.sources.pota import PotaSource, parse_pota
from cockpit.sources.solar import SolarSource, parse_solar

FIXTURES = Path(__file__).parent / "fixtures"
NEIGHBOURS = ["DE", "IT", "SI", "HU", "CZ", "SK", "CH"]


def fixture(name: str) -> bytes:
    return (FIXTURES / name).read_bytes()


@respx.mock
async def test_solar_from_recorded_response() -> None:
    route = respx.get("https://solar.test/x").respond(content=fixture("solar/solarxml.xml"))
    async with make_client() as client:
        solar = await SolarSource(client, "https://solar.test/x", 3600).fetch()
    assert route.calls.last.request.headers["user-agent"] == USER_AGENT
    assert (solar.sfi, solar.ssn, solar.a, solar.k) == (87, 26, 4, 0)
    assert (solar.xray, solar.wind) == ("C1.4", 354)
    assert solar.src_ts == 1795807860  # 27 Nov 2026 19:31 UTC
    assert [(b.b, b.day, b.night) for b in solar.bands][:2] == [
        ("80m-40m", "good", "good"),
        ("30m-20m", "fair", "fair"),
    ]
    assert solar.vhf.aurora is False and solar.vhf.es_eu == "closed"


def test_solar_tolerates_gaps_but_needs_flux() -> None:
    xml = (
        b"<solar><solardata><updated>?</updated><solarflux>90</solarflux>"
        b"<solarwind>NoRpt</solarwind><calculatedconditions>"
        b'<band name="80m-40m" time="day">Good</band></calculatedconditions>'
        b'<calculatedvhfconditions><phenomenon name="vhf-aurora">MID LAT AUR</phenomenon>'
        b'<phenomenon name="E-Skip" location="europe">50MHz ES</phenomenon>'
        b"</calculatedvhfconditions></solardata></solar>"
    )
    solar = parse_solar(xml)
    assert (solar.wind, solar.ssn, solar.src_ts, solar.bands) == (None, None, None, [])
    assert solar.vhf.aurora is True and solar.vhf.es_eu == "50mhz es"
    with pytest.raises(ValueError, match="flux"):
        parse_solar(b"<solar><solardata/></solar>")
    with pytest.raises(ValueError, match="solardata"):
        parse_solar(b"<solar/>")


def test_solar_rejects_entity_expansion() -> None:
    xml = b'<!DOCTYPE s [<!ENTITY a "aaaa">]><solar><solardata>&a;</solardata></solar>'
    with pytest.raises(ValueError):
        parse_solar(xml)


@respx.mock
async def test_pota_from_recorded_response_keeps_europe() -> None:
    respx.get("https://pota.test/s").respond(content=fixture("pota/activator.json"))
    async with make_client() as client:
        pota = await PotaSource(client, "https://pota.test/s", 90, EUROPE, NEIGHBOURS).fetch()
    assert {spot.ref.split("-")[0] for spot in pota.spots} == {"RS", "IE"}
    irish = next(s for s in pota.spots if s.ref == "IE-0212")
    assert (irish.f, irish.b, irish.m) == (7048, "40m", "FT4")
    assert pota.src_ts == max(s.t for s in pota.spots)


def test_pota_sorting_and_broken_entries() -> None:
    base = json.loads(fixture("pota/activator.json"))[0]

    def spot(ref: str, loc: str, minute: int = 0, **extra: object) -> dict[str, object]:
        time = f"2026-11-27T10:{minute:02d}:00"
        return {**base, "reference": ref, "locationDesc": loc, "spotTime": time, **extra}

    data = [
        spot("FR-0001", "FR-ARA", 59),
        spot("CH-0001", "CH-ZH"),
        spot("DE-0001", "DE-BY"),
        spot("AT-0002", "AT-ST"),
        spot("AT-0003", "AT-SZ,AT-KA"),
        spot("AT-0004", "AT-KA", 5),
        spot("US-0001", "US-NY"),
        spot("AT-0005", "AT-KA", invalid=True),
        spot("AT-0006", "AT-KA", frequency="abc"),
        spot("AT-0007", "AT-KA", frequency="9999"),
        "junk",
    ]
    refs = [s.ref for s in parse_pota(data, EUROPE, NEIGHBOURS).spots]
    assert refs == ["AT-0004", "AT-0003", "AT-0002", "DE-0001", "CH-0001", "FR-0001"]
    with pytest.raises(ValueError):
        parse_pota({}, EUROPE, NEIGHBOURS)


def test_news_from_recorded_feed() -> None:
    news = parse_news(fixture("news/feed.xml"))
    assert len(news.items) == 8
    assert news.items[0].title.startswith("Fernsehbild aus einem 10-Cent-Chip")
    assert news.items[0].url.startswith("https://oeradio.at/")
    assert [i.t for i in news.items] == sorted((i.t for i in news.items), reverse=True)
    assert all(len(i.title) <= 90 for i in news.items)


def test_news_rejects_non_feed() -> None:
    with pytest.raises(ValueError):
        parse_news(b"<html><body>maintenance</body></html>")


@respx.mock
async def test_news_uses_etag() -> None:
    route = respx.get("https://news.test/feed")
    route.side_effect = [
        httpx.Response(200, content=fixture("news/feed.xml"), headers={"etag": '"1"'}),
        httpx.Response(304),
        httpx.Response(500),
    ]
    async with make_client() as client:
        source = NewsSource(client, "https://news.test/feed", 1800)
        first = await source.fetch()
        second = await source.fetch()
        with pytest.raises(httpx.HTTPStatusError):
            await source.fetch()
    assert second.items == first.items
    assert route.calls[1].request.headers["if-none-match"] == '"1"'
