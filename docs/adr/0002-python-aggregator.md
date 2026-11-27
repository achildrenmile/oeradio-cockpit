# 0002 – Aggregator in Python

- **Status:** Accepted
- **Datum:** 2026-11-27

## Kontext
Der Aggregator holt heterogene Quellen (XML, REST, Telnet, MQTT, RSS) und publiziert normalisiertes JSON. Zur Wahl standen .NET und Python.

## Entscheidung
Python 3.12 mit asyncio, `aiomqtt`, `httpx`, `pydantic` v2, `uv`, `ruff`, `pytest`. Die JSON Schemas für die Firmware werden aus den pydantic-Modellen generiert.

## Alternativen
- .NET: vertraut, robust, aber das Ham-Radio-Ökosystem (Cluster-Parser, Feeds, Beispiele) ist überwiegend Python, und die Community kann leichter beitragen.

## Konsequenzen
- Sehr niedrige Einstiegshürde für Beiträge aus der Community
- Typdisziplin über `mypy --strict` und pydantic sicherstellen
