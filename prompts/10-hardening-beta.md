# Phase 10 – Härtung und Beta

Lies zuerst alle Dokumente in `docs/` und verschaffe dir einen Überblick über den gesamten Code.

## Ziel
Die v0.1.0-Beta ist für die ersten 10–20 OMs bereit.

## Aufgaben
1. **Review-Durchgang** über das ganze Repo: Secrets (gitleaks, manuell), Lizenz-Header bzw. Attributions vollständig, tote TODOs, Konsistenz von Schema, Doku und Code.
2. **Lasttest Broker:** Skript, das 200 simulierte WSS-Clients (asyncio) gegen die Compose-Umgebung verbindet, und Messung von CPU, RAM und Bandbreite. Ergebnis in `docs/capacity.md`, mit Hochrechnung für den Upload über Starlink.
3. **Robustheit Firmware:** 24-h-Dauerlauf-Checkliste (Heap-Verlauf loggen, keine Leaks), Verhalten bei WLAN-Verlust, Broker weg, Cloudflare-Abriss und DNS-Fehler. Watchdog aktiv.
4. **Robustheit Aggregator:** Ausfall jeder einzelnen Quelle simulieren (Tests), `status.ok` stimmt, die anderen Quellen laufen weiter.
5. **Doku für Nutzer:** `docs/user-guide.md` auf Deutsch, kurz und mit Screenshots: Flashen, Erststart, Bedienung, Filter, Fehlerbilder. Als Grundlage für einen Artikel auf oeradio.at.
6. **Datenschutz-Hinweis** `docs/privacy.md`: welche Daten das Gerät sendet (nur MQTT-Verbindung, Client-ID aus MAC-Suffix, keine Telemetrie), was der Server protokolliert.
7. Release `v0.1.0-beta.1` vorbereiten (Changelog, Release Notes). **Nicht selbst taggen**, sondern Michael bitten.

## Akzeptanzkriterien
- Alle Checklisten abgehakt, Lasttest-Ergebnis dokumentiert
- User-Guide und Privacy-Dokument vorhanden
- Offene Punkte als GitHub-Issues formuliert (Liste im PR)
