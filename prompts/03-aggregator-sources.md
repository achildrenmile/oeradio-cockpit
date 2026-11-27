# Phase 03 – Aggregator-Datenquellen

Lies zuerst `CLAUDE.md`, `docs/mqtt-topics.md`, `aggregator/README.md` und den Code aus Phase 02.

## Ziel
Echte Datenquellen für Solar, POTA, DX-Cluster, News und Mesh, jede robust, höflich zur Quelle und mit Tests gegen aufgezeichnete Fixtures.

## Allgemeine Regeln
- **Recherchiere die aktuellen Endpunkte und Formate selbst** (Web), statt dich auf Erinnerung zu verlassen. Dokumentiere sie in `aggregator/docs/sources.md` mit Datum.
- Pro Quelle: Fixture-Dateien unter `tests/fixtures/<quelle>/` (echte Beispielantwort, gekürzt), Parser-Tests mit `respx` bzw. einem Fake-Stream
- `User-Agent: oeradio-cockpit/<version> (+https://oeradio.at)`, Timeouts, Backoff bei Fehlern, Intervall-Defaults aus `docs/architecture.md`
- Jede Quelle einzeln per Konfiguration aktivierbar

## Quellen
1. **solar** – hamqsl.com Solar-XML (N0NBH): SFI, SSN, A, K, X-Ray, Sonnenwind, HF-Bandbedingungen Tag/Nacht, VHF-Hinweise → Modell `Solar`. XML mit `defusedxml` parsen.
2. **pota** – POTA-Spot-API (aktive Spots): Filter auf Europa per Präfixliste (konfigurierbar), Sortierung OE zuerst und dann die konfigurierten Nachbarn, Bandableitung aus der Frequenz (eigene Bandtabelle `cockpit/bands.py`, IARU Region 1), Kürzung auf `max`.
3. **dx** – DX-Cluster per Telnet (asyncio-Streams): Host, Port und Login-Rufzeichen konfigurierbar (Default-Rufzeichen aus Env, **nicht** im Code). Spot-Zeilen (`DX de …`) parsen, Mode aus Frequenz und Kommentar heuristisch, DXCC-Name über eine Präfix-Tabelle (Lizenz der Tabelle prüfen und in `sources.md` vermerken, z. B. cty.dat von AD1C), Dedupe 5 min, Bündelung 15 s, automatischer Reconnect.
4. **news** – oeradio.at-Feed: URL konfigurierbar. RSS/Atom mit `feedparser`, alternativ JSON. Max. 8 Einträge, Titel max. 90 Zeichen.
5. **mesh** – MeshCore Observer: Subscribe auf den bestehenden Observer-Broker (Host, Topics und Credentials konfigurierbar, eigene Client-Instanz). Zählt Nodes und Repeater, die innerhalb eines konfigurierbaren Fensters gesehen wurden, und hält die letzten Nachrichten **nur aus öffentlichen Kanälen** (Whitelist der Kanalnamen in der Konfiguration). Das Topic-Format des Observers ist zu Beginn unbekannt: Baue einen austauschbaren Parser mit Adapter-Interface und einen Adapter auf Basis eines Beispiel-Dumps, den Michael liefert. Bis dahin Fixture plus TODO und eine klare Fehlermeldung in den Logs.

## Akzeptanzkriterien
- Alle Tests grün, Parser-Abdeckung ≥ 90 %
- `COCKPIT_SOURCES=solar,pota,dx,news` gegen die echten Dienste lokal: Topics füllen sich, `status.ok` stimmt
- `aggregator/docs/sources.md` beschreibt jede Quelle (URL, Format, Intervall, Lizenz/Nutzungsbedingungen)

## Nicht-Ziele
Keine Persistenz und keine Datenbank. Der Zustand lebt im Speicher und in den Retained Messages.
