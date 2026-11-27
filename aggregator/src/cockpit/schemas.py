"""Export the JSON Schemas of all topics: ``python -m cockpit.schemas export``."""

import argparse
import json
from collections.abc import Sequence
from pathlib import Path
from typing import Any

from .models import TOPIC_MODELS

# Resolves to <repo>/schemas/v1 for a source checkout, the only place exporting makes sense.
DEFAULT_DIR = Path(__file__).resolve().parents[3] / "schemas" / "v1"


def schema_for(topic: str) -> dict[str, Any]:
    schema = TOPIC_MODELS[topic].model_json_schema(by_alias=True)
    # "v" has a default in Python but is mandatory on the wire.
    schema["required"] = ["v", *(name for name in schema["required"] if name != "v")]
    return {"$schema": "https://json-schema.org/draft/2020-12/schema", **schema}


def export(directory: Path = DEFAULT_DIR) -> list[Path]:
    directory.mkdir(parents=True, exist_ok=True)
    written = []
    for topic in TOPIC_MODELS:
        path = directory / f"{topic}.schema.json"
        content = json.dumps(schema_for(topic), indent=2, ensure_ascii=False)
        path.write_text(content + "\n", encoding="utf-8")
        written.append(path)
    return written


def main(argv: Sequence[str] | None = None) -> None:
    parser = argparse.ArgumentParser(prog="cockpit.schemas")
    parser.add_argument("command", choices=["export"])
    parser.add_argument("--out", type=Path, default=DEFAULT_DIR)
    args = parser.parse_args(argv)
    for path in export(args.out):
        print(path)


if __name__ == "__main__":
    main()
