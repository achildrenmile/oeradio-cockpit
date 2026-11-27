# CLAUDE.md – Arbeitsregeln für Agents in diesem Repo

Dieses Repo ist **öffentlich**. Alles, was hier landet, ist für die Welt sichtbar.

## Grundsätze

- **Der Vertrag zuerst.** `schemas/` und `docs/mqtt-topics.md` sind die Schnittstelle zwischen
  Aggregator und Firmware. Wer sie ändert, ändert beide Seiten im selben PR und erhöht die
  Schema-Version, wenn die Änderung nicht abwärtskompatibel ist.
- **Entscheidungen als ADR.** Architekturentscheidungen kommen als neue Datei nach `docs/adr/`
  (Format siehe `0000-template.md`). Bestehende ADRs werden nicht umgeschrieben, sondern durch
  neue ersetzt (`Status: Superseded by NNNN`).
- **Keine Secrets.** Keine Tokens, Passwörter, Tunnel-Credentials, WLAN-Daten oder privaten
  Hostnamen im Repo. Beispiele heißen `*.example.*`. Prüfe vor jedem Commit.
- **Klein und testbar.** Kleine Commits mit Conventional Commits (`feat:`, `fix:`, `docs:` …).
  Jede Phase in `prompts/` endet mit einem grünen Build bzw. grünen Tests.
- **Sprache.** Code, Bezeichner und Code-Kommentare auf Englisch. Doku, ADRs und UI-Texte auf
  Deutsch (UI-Texte zentral, damit später übersetzbar).

## Komponenten

### aggregator/ (Python)
- Python 3.12, `uv` für Abhängigkeiten, `ruff` (Lint + Format), `pytest`, `mypy --strict`
- asyncio-Architektur: eine Task pro Datenquelle, gemeinsamer MQTT-Publisher
- `pydantic` v2 für Nachrichtenmodelle. Die JSON Schemas in `schemas/` werden aus den Modellen
  generiert (`uv run python -m cockpit.schemas export`) und eingecheckt. Die CI prüft auf Drift.
- Alle Quellen-URLs, Intervalle und Filter sind konfigurierbar (Env-Variablen mit Präfix `COCKPIT_`)
- Externe Dienste schonen: Intervalle einhalten, `User-Agent: oeradio-cockpit/<version> (+https://oeradio.at)`,
  Backoff bei Fehlern, keine parallelen Doppelabfragen

### firmware/ (ESP-IDF)
- ESP-IDF 5.x, Ziel `esp32s3`, LVGL 9 über `esp_lvgl_port` (ESP Component Registry)
- MQTT ausschließlich über `esp-mqtt` mit `wss://` und `esp_crt_bundle` (kein Zertifikats-Pinning)
- Pins und Hardware-Init des T-Deck **immer gegen das offizielle LilyGO-Repo verifizieren**
  (`Xinyuan-LilyGO/T-Deck`, `examples/*/utilities.h`), nicht raten
- Display 320×240: große Ziffern, wenig Text pro Seite, alle Strings UTF-8 inkl. Umlauten
- Komponentenstruktur: `components/board`, `components/net`, `components/store`, `main/ui/pages/*`

### deploy/
- Kustomize-Basis als **Referenz**. Das produktive Deployment läuft im privaten GitOps-Repo, das
  diese Basis referenziert. Secrets nur als `*.example.yaml`.

## Definition of Done (jede Phase)
1. Build bzw. Tests lokal und in der CI grün
2. Doku aktualisiert (README des Moduls, ggf. ADR, Topic-Doku)
3. Keine Secrets im Diff
4. Kurze Zusammenfassung im PR: Was, warum, wie getestet
