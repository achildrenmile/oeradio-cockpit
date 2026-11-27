"""Runtime configuration from ``COCKPIT_*`` environment variables."""

from typing import Annotated, Literal

from pydantic import Field, SecretStr, field_validator
from pydantic_settings import BaseSettings, NoDecode, SettingsConfigDict

DATA_TOPICS = ("solar", "pota", "dx", "mesh", "news")


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_prefix="COCKPIT_")

    broker_host: str = "localhost"
    broker_port: int = Field(default=1883, ge=1, le=65535)
    broker_user: str | None = None
    broker_password: SecretStr | None = None
    broker_tls: bool = False
    topic_prefix: str = "oeradio/cockpit/v1"

    # Comma separated list. "demo" produces fake data for all data topics.
    sources: Annotated[list[str], NoDecode] = ["demo"]

    interval_solar: float = Field(default=1800, gt=0)
    interval_pota: float = Field(default=90, gt=0)
    interval_dx: float = Field(default=15, gt=0)
    interval_mesh: float = Field(default=30, gt=0)
    interval_news: float = Field(default=1800, gt=0)
    interval_status: float = Field(default=30, gt=0)

    motd: str | None = None
    log_level: Literal["DEBUG", "INFO", "WARNING", "ERROR"] = "INFO"

    @field_validator("sources", mode="before")
    @classmethod
    def _split_sources(cls, value: object) -> object:
        if isinstance(value, str):
            return [item.strip() for item in value.split(",") if item.strip()]
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
