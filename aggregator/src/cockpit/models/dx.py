from typing import TYPE_CHECKING, Annotated, Self

from pydantic import BaseModel, Field, model_validator

from .base import Message, capped, text

MAX_SPOTS = 40
COMMENT_MAX = 30

if TYPE_CHECKING:
    Comment = str
else:
    Comment = text(COMMENT_MAX)


class DxSpot(BaseModel):
    t: int
    dx: str
    f: int
    b: str
    m: str | None = None
    de: str
    cmt: Comment | None = None
    dxcc: str | None = None


class Dx(Message):
    max: int = Field(default=MAX_SPOTS, le=MAX_SPOTS)
    spots: Annotated[list[DxSpot], capped(MAX_SPOTS), Field(max_length=MAX_SPOTS)]

    @model_validator(mode="after")
    def _fit_buffer(self) -> Self:
        self.drop_tail_to_fit(self.spots)
        return self
