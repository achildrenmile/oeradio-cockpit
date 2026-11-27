"""CLI: ``python -m cockpit run`` and ``python -m cockpit schemas export``."""

import asyncio
import logging
import sys

import structlog

from . import health, schemas
from .app import run
from .config import Settings

USAGE = "usage: python -m cockpit (run | healthcheck | schemas export [--out DIR])"


def main(argv: list[str] | None = None) -> None:
    args = sys.argv[1:] if argv is None else argv
    if args[:1] == ["schemas"]:
        schemas.main(args[1:])
        return
    if args == ["healthcheck"]:
        settings = Settings()
        path = settings.health_file
        sys.exit(0 if path and health.is_alive(path, settings.interval_status) else 1)
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
