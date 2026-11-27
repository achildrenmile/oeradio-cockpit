from typing import Literal

from pydantic import BaseModel

from .base import Message

Condition = Literal["good", "fair", "poor"]


class BandCondition(BaseModel):
    b: str
    day: Condition
    night: Condition


class Vhf(BaseModel):
    aurora: bool
    es_eu: str


class Solar(Message):
    sfi: int
    ssn: int | None = None
    a: int | None = None
    k: int | None = None
    xray: str | None = None
    wind: int | None = None
    bands: list[BandCondition]
    vhf: Vhf
