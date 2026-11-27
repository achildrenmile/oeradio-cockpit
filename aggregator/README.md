# Aggregator

Der Python-Dienst `cockpit` publiziert kompaktes JSON per MQTT (retained, QoS 1) nach dem
[Topic-Vertrag](../docs/mqtt-topics.md), siehe auch [ADR 0002](../docs/adr/0002-python-aggregator.md).
Quellen: `solar`, `pota`, `dx`, `mesh`, `news` ([Details](docs/sources.md)) und `demo` mit erfundenen Daten.
Das Container-Image folgt in [Phase 04](../prompts/04-broker-deploy.md).

## Lokal starten

Voraussetzungen: [`uv`](https://docs.astral.sh/uv/) und Docker.

```sh
# Terminal 1: Broker ohne Anmeldung, nur auf dem eigenen Rechner erreichbar
docker run --rm -p 127.0.0.1:1883:1883 eclipse-mosquitto:2 mosquitto -c /mosquitto-no-auth.conf

# Terminal 2: Aggregator
cd aggregator
uv sync
COCKPIT_SOURCES=demo uv run python -m cockpit run

# Terminal 3: mitlesen (mosquitto_sub aus dem Paket mosquitto bzw. mosquitto-clients)
mosquitto_sub -t 'oeradio/cockpit/#' -v
```

Mit echten Daten (für `dx` ein eigenes Rufzeichen setzen):

```sh
COCKPIT_SOURCES=solar,pota,dx,news COCKPIT_DX_CALLSIGN=<rufzeichen> uv run python -m cockpit run
```

Beim Beenden (Strg+C oder SIGTERM) publiziert der Dienst `offline` auf `…/status`.

## Konfiguration

| Variable | Default | Bedeutung |
|---|---|---|
| `COCKPIT_BROKER_HOST` | `localhost` | MQTT-Broker |
| `COCKPIT_BROKER_PORT` | `1883` | Port |
| `COCKPIT_BROKER_USER` | – | Benutzer |
| `COCKPIT_BROKER_PASSWORD` | – | Passwort (nie ins Repo) |
| `COCKPIT_BROKER_TLS` | `false` | TLS mit den System-Zertifikaten |
| `COCKPIT_TOPIC_PREFIX` | `oeradio/cockpit/v1` | Topic-Präfix |
| `COCKPIT_SOURCES` | `demo` | Aktive Quellen, mit Komma getrennt; `demo` füllt die nicht genannten Topics |
| `COCKPIT_INTERVAL_SOLAR` | `3600` | Sekunden |
| `COCKPIT_INTERVAL_POTA` | `90` | Sekunden |
| `COCKPIT_INTERVAL_DX` | `15` | Sekunden |
| `COCKPIT_INTERVAL_MESH` | `30` | Sekunden |
| `COCKPIT_INTERVAL_NEWS` | `1800` | Sekunden |
| `COCKPIT_INTERVAL_STATUS` | `30` | Sekunden |
| `COCKPIT_MOTD` | – | Kurzmeldung im Status (max. 80 Zeichen) |
| `COCKPIT_LOG_LEVEL` | `INFO` | `DEBUG`, `INFO`, `WARNING`, `ERROR` |
| `COCKPIT_SOLAR_URL`, `COCKPIT_POTA_URL`, `COCKPIT_NEWS_URL` | siehe Quellen | Endpunkte |
| `COCKPIT_POTA_EUROPE` | europäische Programme | POTA-Präfixe, die behalten werden |
| `COCKPIT_POTA_NEIGHBOURS` | `DE,IT,SI,HU,CZ,SK,CH` | Reihenfolge nach Österreich |
| `COCKPIT_DX_HOST` / `_PORT` | `dxspider.co.uk` / `7300` | DX-Cluster |
| `COCKPIT_DX_CALLSIGN` | – | Pflicht für `dx` |
| `COCKPIT_MESH_HOST` / `_PORT` | – / `1883` | Observer-Broker, Host ist Pflicht für `mesh` |
| `COCKPIT_MESH_USER` / `_PASSWORD` / `_TLS` | – / – / `false` | Anmeldung (nie ins Repo) |
| `COCKPIT_MESH_TOPIC` | `meshcore/+/+/packets` | Paket-Topic |
| `COCKPIT_MESH_CHANNELS` | `public` | `public` und `#kanal`, mit Komma getrennt |
| `COCKPIT_MESH_WINDOW_HOURS` | `24` | Fenster für „online“ |

## Entwickeln

```sh
uv run ruff check
uv run ruff format
uv run mypy --strict src
uv run pytest
uv run python -m cockpit schemas export   # nach Änderungen an den Modellen
```

`schemas/v1/*.schema.json` wird aus `src/cockpit/models/` generiert und eingecheckt. Die CI prüft auf Drift.
