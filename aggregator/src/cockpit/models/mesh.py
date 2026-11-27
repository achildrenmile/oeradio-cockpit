from typing import TYPE_CHECKING, Annotated

from pydantic import BaseModel, Field

from .base import Message, capped, text

TXT_MAX = 80
MAX_RECENT = 10
MAX_REPEATERS = 5

if TYPE_CHECKING:
    Txt = str
else:
    Txt = text(TXT_MAX)


class MeshMessage(BaseModel):
    """A message from a public channel. Direct messages must never end up here."""

    model_config = {"populate_by_name": True}

    t: int
    sender: str = Field(alias="from")
    ch: str
    txt: Txt


class Repeater(BaseModel):
    name: str
    seen: int


class Mesh(Message):
    nodes_online: int = Field(ge=0)
    repeaters_online: int = Field(ge=0)
    recent: Annotated[list[MeshMessage], capped(MAX_RECENT), Field(max_length=MAX_RECENT)]
    top_repeaters: Annotated[list[Repeater], capped(MAX_REPEATERS), Field(max_length=MAX_REPEATERS)]
