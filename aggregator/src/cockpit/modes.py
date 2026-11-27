"""Guess the mode of a DX spot from its comment and frequency."""

import re

_KEYWORDS = re.compile(r"\b(CW|FT8|FT4|RTTY|PSK31|PSK|SSB|USB|LSB|FM)\b", re.IGNORECASE)
_ALIASES = {"USB": "SSB", "LSB": "SSB", "PSK31": "PSK"}

# Usual dial frequencies; signals sit up to 3 kHz above.
_FT8 = (1840, 3573, 7074, 10136, 14074, 18100, 21074, 24915, 28074, 50313)
_FT4 = (3575, 7047, 10140, 14080, 18104, 21140, 24919, 28180, 50318)

# Upper end of the CW segment per band (IARU Region 1).
_CW_END = {
    "160m": 1838,
    "80m": 3570,
    "40m": 7040,
    "30m": 10130,
    "20m": 14070,
    "17m": 18095,
    "15m": 21070,
    "12m": 24915,
    "10m": 28070,
    "6m": 50100,
}


def guess_mode(khz: float, band: str, comment: str) -> str | None:
    if match := _KEYWORDS.search(comment):
        word = match.group(1).upper()
        return _ALIASES.get(word, word)
    for mode, dials in (("FT8", _FT8), ("FT4", _FT4)):
        if any(dial <= khz <= dial + 3 for dial in dials):
            return mode
    end = _CW_END.get(band)
    if end is None or (band == "30m" and khz > end):
        return None  # no basis for a guess (30m has no phone)
    if khz < end:
        return "CW"
    # The digital segment sits between CW and phone; do not call it SSB.
    return "SSB" if khz >= end + 30 or band in ("160m", "80m", "40m") else None
