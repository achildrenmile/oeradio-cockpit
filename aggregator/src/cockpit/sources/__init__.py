"""Source registry."""

import aiomqtt
import httpx

from ..config import DATA_TOPICS, Settings
from ..dxcc import default_table
from .base import Source, SourceRunner
from .demo import DemoSource
from .dx import DxSource
from .mesh import MeshSource
from .meshcore import Channel, PacketCaptureAdapter
from .news import NewsSource
from .pota import PotaSource
from .solar import SolarSource


def _mesh(settings: Settings) -> MeshSource:
    host = settings.mesh_host
    if not host:
        raise ValueError("the mesh source needs COCKPIT_MESH_HOST")
    password = settings.mesh_password

    def client() -> aiomqtt.Client:
        return aiomqtt.Client(
            hostname=host,
            port=settings.mesh_port,
            username=settings.mesh_user,
            password=password.get_secret_value() if password else None,
            tls_params=aiomqtt.TLSParameters() if settings.mesh_tls else None,
        )

    adapter = PacketCaptureAdapter([Channel.from_name(n) for n in settings.mesh_channels])
    return MeshSource(
        client,
        settings.mesh_topic,
        adapter,
        settings.interval_mesh,
        settings.mesh_window_hours * 3600,
    )


def _build(topic: str, settings: Settings, http: httpx.AsyncClient) -> Source:
    interval = settings.interval_for(topic)
    match topic:
        case "solar":
            return SolarSource(http, settings.solar_url, interval)
        case "pota":
            return PotaSource(
                http, settings.pota_url, interval, settings.pota_europe, settings.pota_neighbours
            )
        case "dx":
            if not settings.dx_callsign:
                raise ValueError("the dx source needs COCKPIT_DX_CALLSIGN")
            return DxSource(
                settings.dx_host,
                settings.dx_port,
                settings.dx_callsign,
                interval,
                default_table(),
            )
        case "mesh":
            return _mesh(settings)
        case "news":
            return NewsSource(http, settings.news_url, interval)
    raise ValueError(f"unknown source: {topic}")


def build_sources(settings: Settings, http: httpx.AsyncClient) -> list[Source]:
    """Named sources are real; ``demo`` fills the remaining topics."""
    enabled = set(settings.sources)
    unknown = sorted(enabled - {*DATA_TOPICS, "demo"})
    if unknown:
        raise ValueError(f"unknown sources: {', '.join(unknown)}")
    sources: list[Source] = []
    for topic in DATA_TOPICS:
        if topic in enabled:
            sources.append(_build(topic, settings, http))
        elif "demo" in enabled:
            sources.append(DemoSource(topic, settings.interval_for(topic)))
    return sources


__all__ = ["DemoSource", "Source", "SourceRunner", "build_sources"]
