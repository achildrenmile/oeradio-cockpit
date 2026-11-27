from typing import TYPE_CHECKING, Annotated, Self

from pydantic import BaseModel, Field, model_validator

from .base import Message, capped, text

MAX_SPOTS = 20
PARK_MAX = 40
COMMENT_MAX = 30

if TYPE_CHECKING:
    Park = str
    Comment = str
else:
    Park = text(PARK_MAX)
    Comment = text(COMMENT_MAX)


class PotaSpot(BaseModel):
    id: int
    t: int
    act: str
    ref: str
    park: Park | None = None
    loc: str | None = None
    f: int
    b: str
    m: str | None = None
    sp: str
    cmt: Comment | None = None


class Pota(Message):
    max: int = Field(default=MAX_SPOTS, le=MAX_SPOTS)
    spots: Annotated[list[PotaSpot], capped(MAX_SPOTS), Field(max_length=MAX_SPOTS)]

    @model_validator(mode="after")
    def _fit_buffer(self) -> Self:
        self.drop_tail_to_fit(self.spots)
        return self
