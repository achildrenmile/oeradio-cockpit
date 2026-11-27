"""CLI: ``python -m cockpit run`` and ``python -m cockpit schemas export``."""

import asyncio
import logging
import sys

import structlog

from . import schemas
from .app import run
from .config import Settings

USAGE = "usage: python -m cockpit (run | schemas export [--out DIR])"


def main(argv: list[str] | None = None) -> None:
    args = sys.argv[1:] if argv is None else argv
    if args[:1] == ["schemas"]:
        schemas.main(args[1:])
        return
    if args != ["run"]:
        sys.exit(USAGE)
    settings = Settings()
    structlog.configure(
        wrapper_class=structlog.make_filtering_bound_logger(
            logging.getLevelNamesMapping()[settings.log_level]
        )
    )
    asyncio.run(run(settings))


if __name__ == "__main__":
    main()
