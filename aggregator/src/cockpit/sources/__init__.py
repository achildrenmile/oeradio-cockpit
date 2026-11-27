"""Source registry. Real sources are added in phase 03."""

from ..config import DATA_TOPICS, Settings
from .base import Source, SourceRunner
from .demo import DemoSource


def build_sources(settings: Settings) -> list[Source]:
    unknown = sorted(set(settings.sources) - {"demo"})
    if unknown:
        raise ValueError(f"unknown sources: {', '.join(unknown)} (available: demo)")
    if "demo" not in settings.sources:
        return []
    return [DemoSource(topic, settings.interval_for(topic)) for topic in DATA_TOPICS]


__all__ = ["DemoSource", "Source", "SourceRunner", "build_sources"]
