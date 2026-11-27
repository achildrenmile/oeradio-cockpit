"""Exponential backoff."""


def backoff_delay(failures: int, base: float, cap: float) -> float:
    """Delay after ``failures`` consecutive errors: base, 2*base, 4*base ... up to cap."""
    if failures <= 0:
        return base
    return float(min(base * 2 ** min(failures - 1, 16), max(cap, base)))
