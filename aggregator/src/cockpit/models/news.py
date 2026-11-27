from typing import TYPE_CHECKING, Annotated

from pydantic import BaseModel, Field

from .base import Message, capped, text

TITLE_MAX = 90
MAX_ITEMS = 8

if TYPE_CHECKING:
    Title = str
else:
    Title = text(TITLE_MAX)


class NewsItem(BaseModel):
    t: int
    title: Title
    url: str


class News(Message):
    items: Annotated[list[NewsItem], capped(MAX_ITEMS), Field(max_length=MAX_ITEMS)]
