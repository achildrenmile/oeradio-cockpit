"""Common message base and length limiting helpers."""

import time
from typing import Annotated, Any, Literal

from pydantic import BaseModel, BeforeValidator, Field

ELLIPSIS = "…"
WARN_BYTES = 4 * 1024  # size target
MAX_BYTES = 8 * 1024  # hard firmware buffer


def truncate(value: str, limit: int) -> str:
    """Shorten ``value`` to at most ``limit`` characters, marking the cut."""
    if len(value) <= limit:
        return value
    return value[: limit - 1].rstrip() + ELLIPSIS


def text(limit: int) -> Any:
    """Annotated string type that truncates instead of rejecting long input."""

    def _cut(value: object) -> object:
        return truncate(value, limit) if isinstance(value, str) else value

    return Annotated[str, BeforeValidator(_cut), Field(max_length=limit)]


def capped(limit: int) -> BeforeValidator:
    """Keep only the first ``limit`` items; sources deliver lists pre-sorted."""

    def _cut(value: object) -> object:
        return value[:limit] if isinstance(value, list) else value

    return BeforeValidator(_cut)


def now() -> int:
    return int(time.time())


class Message(BaseModel):
    v: Literal[1] = 1
    ts: int
    src_ts: int | None = None

    def encoded(self) -> bytes:
        """Compact wire format."""
        return self.model_dump_json(exclude_none=True, by_alias=True).encode()

    def drop_tail_to_fit(self, items: list[Any]) -> None:
        """Remove the lowest priority (last) items until the message fits the buffer."""
        while items and len(self.encoded()) > MAX_BYTES:
            items.pop()
