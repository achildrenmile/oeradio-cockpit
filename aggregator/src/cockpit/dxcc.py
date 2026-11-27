"""DXCC entity lookup based on cty.dat (see data/LICENSE-cty.txt)."""

import re
from functools import lru_cache
from importlib import resources

_OVERRIDES = re.compile(r"\([^)]*\)|\[[^\]]*\]|<[^>]*>|\{[^}]*\}|~[^~]*~")
_IGNORED_SUFFIXES = {"P", "M", "QRP", "A", "LH"}


class DxccTable:
    def __init__(self, text: str) -> None:
        self._exact: dict[str, str] = {}
        self._prefixes: dict[str, str] = {}
        for record in text.split(";"):
            fields = record.split(":")
            if len(fields) < 9:
                continue
            name, primary = fields[0].strip(), fields[7].strip()
            if primary.startswith("*"):
                continue  # contest-only entities, not DXCC
            for alias in fields[8].split(","):
                alias = _OVERRIDES.sub("", alias).strip()
                if alias.startswith("="):
                    self._exact[alias[1:]] = name
                elif alias:
                    self._prefixes[alias] = name

    def lookup(self, call: str) -> str | None:
        call = call.upper()
        if call in self._exact:
            return self._exact[call]
        parts = call.split("/")
        if parts[-1] in ("MM", "AM"):
            return None  # at sea or in the air: no entity
        parts = [p for p in parts if p and p not in _IGNORED_SUFFIXES and not p.isdigit()]
        if not parts:
            return None
        # In "VP8/OE1ABC" the shorter part names the location.
        base = min(parts, key=len)
        for length in range(len(base), 0, -1):
            if name := self._prefixes.get(base[:length]):
                return name
        return None


@lru_cache(maxsize=1)
def default_table() -> DxccTable:
    path = resources.files("cockpit") / "data" / "cty.dat"
    return DxccTable(path.read_text(encoding="latin-1"))
