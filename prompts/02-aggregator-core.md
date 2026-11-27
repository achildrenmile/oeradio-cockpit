# Phase 02 – Aggregator-Kern und Schemas

Lies zuerst `CLAUDE.md`, `docs/architecture.md`, `docs/mqtt-topics.md`, ADR 0002, 0003 und 0005.

## Ziel
Ein lauffähiger Python-Dienst `cockpit` (in `aggregator/`) mit Konfiguration, MQTT-Publisher,
Status-Topic, Datenmodellen aller Topics und generierten JSON Schemas. Echte Datenquellen kommen erst in Phase 03.
Hier gibt es eine `demo`-Quelle mit Fake-Daten.

## Aufgaben
1. Projekt mit `uv` anlegen: `aggregator/pyproject.toml`, Paket `src/cockpit/`, Python 3.12.
   Abhängigkeiten: `aiomqtt`, `httpx`, `pydantic>=2`, `pydantic-settings`, `structlog`. Dev: `pytest`, `pytest-asyncio`, `ruff`, `mypy`, `respx`.
2. `cockpit/config.py`: `pydantic-settings`, Präfix `COCKPIT_`. Broker-Host, Port, User, Passwort, TLS an/aus,
   Topic-Präfix (Default `oeradio/cockpit/v1`), Intervalle je Quelle, aktivierte Quellen, Log-Level.
3. `cockpit/models/`: pydantic-Modelle exakt nach `docs/mqtt-topics.md` (`Status`, `Solar`, `Pota`, `Dx`, `Mesh`, `News`).
   Gemeinsame Basis mit `v`, `ts`, `src_ts`. Längenbegrenzungen (Kürzungen von `txt`, `title`, `max`) in Validatoren bzw. Helfern.
4. `cockpit/publisher.py`: ein MQTT-Client für alle Quellen, publiziert **retained, QoS 1**, kompaktes JSON
   (`model_dump_json(exclude_none=True)`), prüft die Payload-Größe (Warnung > 4 KB, Abbruch > 8 KB).
   Last Will auf `…/status` mit `{"v":1,"ts":…,"offline":true}`. Reconnect mit Backoff.
5. `cockpit/sources/base.py`: abstrakte Quelle (`name`, `interval`, `async fetch() -> Model`), Runner mit
   Intervall, Fehlerzählung, Backoff und Zeitstempel des letzten Erfolgs. `cockpit/sources/demo.py` erzeugt plausible Fake-Daten für alle Topics.
6. `cockpit/status.py`: publiziert alle 30 s `Status` mit `ok`-Map (Erfolg und Alter ≤ 3 × Intervall) und optionaler `motd` aus der Konfiguration.
7. `cockpit/__main__.py`: `python -m cockpit run` startet alle aktivierten Quellen, `python -m cockpit schemas export` schreibt
   JSON Schemas nach `../schemas/v1/<topic>.schema.json`.
8. Schemas generieren und committen. In `ci.yml` den Schema-Drift-Check aktivieren (export, dann `git diff --exit-code schemas/`).
9. Tests: Modelle (Kürzung, Größenlimit), Publisher (mit gemocktem Client), Status-Logik (ok/stale), Runner-Backoff.
10. `aggregator/README.md`: lokales Starten mit `docker run eclipse-mosquitto` und `COCKPIT_SOURCES=demo`,
    Mitlesen mit `mosquitto_sub -t 'oeradio/cockpit/#' -v`.

## Akzeptanzkriterien
- `uv run ruff check`, `uv run mypy --strict src`, `uv run pytest` grün
- Mit lokalem Mosquitto erscheinen alle sechs Topics retained mit gültigem JSON
- `schemas/v1/*.schema.json` existiert, der Drift-Check in der CI ist grün

## Nicht-Ziele
Keine echten externen Abfragen (Phase 03), kein Dockerfile (Phase 04).
