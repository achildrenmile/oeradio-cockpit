"""Runtime configuration from ``COCKPIT_*`` environment variables."""

from pathlib import Path
from typing import Annotated, Literal

from pydantic import Field, SecretStr, field_validator
from pydantic_settings import BaseSettings, NoDecode, SettingsConfigDict

DATA_TOPICS = ("solar", "pota", "dx", "mesh", "news")

# POTA program prefixes of European countries.
EUROPE = [
    "AD",
    "AL",
    "AT",
    "AX",
    "BA",
    "BE",
    "BG",
    "BY",
    "CH",
    "CY",
    "CZ",
    "DE",
    "DK",
    "EE",
    "ES",
    "FI",
    "FO",
    "FR",
    "GB",
    "GG",
    "GI",
    "GR",
    "HR",
    "HU",
    "IE",
    "IM",
    "IS",
    "IT",
    "JE",
    "LI",
    "LT",
    "LU",
    "LV",
    "MC",
    "MD",
    "ME",
    "MK",
    "MT",
    "NL",
    "NO",
    "PL",
    "PT",
    "RO",
    "RS",
    "SE",
    "SI",
    "SK",
    "SM",
    "TR",
    "UA",
    "VA",
]


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_prefix="COCKPIT_")

    broker_host: str = "localhost"
    broker_port: int = Field(default=1883, ge=1, le=65535)
    broker_user: str | None = None
    broker_password: SecretStr | None = None
    broker_tls: bool = False
    topic_prefix: str = "oeradio/cockpit/v1"

    # Comma separated list of data topics. "demo" fills all others with fake data.
    sources: Annotated[list[str], NoDecode] = ["demo"]

    solar_url: str = "https://www.hamqsl.com/solarxml.php"
    pota_url: str = "https://api.pota.app/spot/activator"
    pota_europe: Annotated[list[str], NoDecode] = EUROPE
    pota_neighbours: Annotated[list[str], NoDecode] = ["DE", "IT", "SI", "HU", "CZ", "SK", "CH"]
    news_url: str = "https://oeradio.at/feed/"

    dx_host: str = "dxspider.co.uk"
    dx_port: int = Field(default=7300, ge=1, le=65535)
    dx_callsign: str | None = None  # required for the dx source, deliberately no default

    mesh_host: str | None = None
    mesh_port: int = Field(default=1883, ge=1, le=65535)
    mesh_user: str | None = None
    mesh_password: SecretStr | None = None
    mesh_tls: bool = False
    mesh_topic: str = "meshcore/+/+/packets"
    # Only "public" and hashtag channels can be listed, so private ones stay unreadable.
    mesh_channels: Annotated[list[str], NoDecode] = ["public"]
    mesh_window_hours: float = Field(default=24, gt=0)

    interval_solar: float = Field(default=3600, gt=0)
    interval_pota: float = Field(default=90, gt=0)
    interval_dx: float = Field(default=15, gt=0)
    interval_mesh: float = Field(default=30, gt=0)
    interval_news: float = Field(default=1800, gt=0)
    interval_status: float = Field(default=30, gt=0)

    motd: str | None = None
    health_file: Path | None = None  # touched every status interval
    log_level: Literal["DEBUG", "INFO", "WARNING", "ERROR"] = "INFO"

    @field_validator("sources", "pota_europe", "pota_neighbours", "mesh_channels", mode="before")
    @classmethod
    def _split_sources(cls, value: object) -> object:
        if isinstance(value, str):
            return [item.strip() for item in value.split(",") if item.strip()]
        return value

    @field_validator("mesh_channels")
    @classmethod
    def _only_public_channels(cls, value: list[str]) -> list[str]:
        for name in value:
            if name.lower() != "public" and not name.startswith("#"):
                raise ValueError(f"mesh channel {name!r} must be 'public' or start with '#'")
        return value

    @field_validator("topic_prefix")
    @classmethod
    def _strip_slash(cls, value: str) -> str:
        return value.rstrip("/")

    @field_validator("log_level", mode="before")
    @classmethod
    def _upper(cls, value: object) -> object:
        return value.upper() if isinstance(value, str) else value

    def interval_for(self, topic: str) -> float:
        intervals = {
            "solar": self.interval_solar,
            "pota": self.interval_pota,
            "dx": self.interval_dx,
            "mesh": self.interval_mesh,
            "news": self.interval_news,
        }
        return intervals[topic]
