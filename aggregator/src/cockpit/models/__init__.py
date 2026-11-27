"""Pydantic models of all topics, the source of truth for ``schemas/v1``."""

from .base import Message, now, truncate
from .dx import Dx, DxSpot
from .mesh import Mesh, MeshMessage, Repeater
from .news import News, NewsItem
from .pota import Pota, PotaSpot
from .solar import BandCondition, Solar, Vhf
from .status import Status

TOPIC_MODELS: dict[str, type[Message]] = {
    "status": Status,
    "solar": Solar,
    "pota": Pota,
    "dx": Dx,
    "mesh": Mesh,
    "news": News,
}

__all__ = [
    "TOPIC_MODELS",
    "BandCondition",
    "Dx",
    "DxSpot",
    "Mesh",
    "MeshMessage",
    "Message",
    "News",
    "NewsItem",
    "Pota",
    "PotaSpot",
    "Repeater",
    "Solar",
    "Status",
    "Vhf",
    "now",
    "truncate",
]
