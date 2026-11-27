"""Amateur bands (IARU Region 1, with the wider upper edges of other regions) in kHz."""

BANDS: tuple[tuple[int, int, str], ...] = (
    (135, 138, "2200m"),
    (472, 479, "630m"),
    (1800, 2000, "160m"),
    (3500, 4000, "80m"),
    (5250, 5450, "60m"),
    (7000, 7300, "40m"),
    (10100, 10150, "30m"),
    (14000, 14350, "20m"),
    (18068, 18168, "17m"),
    (21000, 21450, "15m"),
    (24890, 24990, "12m"),
    (28000, 29700, "10m"),
    (50000, 54000, "6m"),
    (70000, 70500, "4m"),
    (144000, 148000, "2m"),
    (430000, 450000, "70cm"),
    (1240000, 1300000, "23cm"),
)


def band_for(khz: float) -> str | None:
    """Spot frequencies come from all regions, hence the generous edges."""
    for low, high, name in BANDS:
        if low <= khz <= high:
            return name
    return None
