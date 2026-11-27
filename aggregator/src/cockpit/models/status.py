from typing import TYPE_CHECKING, Self

from pydantic import model_validator

from .base import Message, text

MOTD_MAX = 80

if TYPE_CHECKING:
    Motd = str
else:
    Motd = text(MOTD_MAX)


class Status(Message):
    """Aggregator health. The last will only carries ``v``, ``ts`` and ``offline``."""

    agg: str | None = None
    ok: dict[str, bool] | None = None
    motd: Motd | None = None
    offline: bool | None = None

    @model_validator(mode="after")
    def _complete_unless_offline(self) -> Self:
        if not self.offline and (self.agg is None or self.ok is None):
            raise ValueError("agg and ok are required unless offline is true")
        return self
