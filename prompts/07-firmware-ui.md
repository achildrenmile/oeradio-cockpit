# Phase 07 – Firmware: Seiten (UI)

Lies zuerst `CLAUDE.md`, `docs/mqtt-topics.md` (Abschnitt „Firmware-Verhalten bei fehlenden Daten“), den Code aus Phase 05 und 06 sowie `assets/` (falls Phase 08 schon gemergt ist, sonst Platzhalter verwenden).

## Ziel
Die sieben MVP-Seiten auf 320×240, bedienbar per Touch-Swipe, Trackball und Tastatur, im oeradio.at-Look.

## UI-Rahmen
- **Statusleiste** oben (ca. 20 px): Rufzeichen, Seitenname, WLAN, MQTT, Server, UTC-Uhr, Akku
- **Seitenwechsel:** horizontaler Swipe (`lv_tileview`), Trackball links/rechts, Tastatur `1`–`7` als Direktwahl, `s` für Einstellungen
- **Scrollen in Listen:** Trackball hoch/runter, Klick öffnet die Detailansicht, `Esc`/Backspace schließt sie
- **Theme:** zentrales `ui/theme.c` mit Farben und Fonts aus `assets/`. Strings zentral in `ui/strings_de.h`
- **Datenalter:** Jede Seite zeigt Daten, die älter als 3 × Intervall sind, gedimmt mit „Stand HH:MM UTC“
- **Neu zeichnen nur bei Änderung** des Store-Versionszählers, ein LVGL-Timer prüft 2× pro Sekunde

## Seiten
1. **Uhr:** UTC groß, Lokalzeit darunter, Datum, Locator. Sonnenauf- und -untergang für den Locator lokal berechnet (NOAA-Algorithmus, eigene kleine C-Funktion mit Host-Test). Graulinie als vereinfachte Weltkarte (vorgerendertes, kleines Bitmap aus `assets/`, Tag/Nacht-Overlay berechnet, eigener Standort markiert).
2. **Band:** SFI, K und A groß mit Farbcodierung (K ≥ 5 rot), Bandtabelle Tag/Nacht als Ampel, X-Ray und Sonnenwind klein.
3. **POTA:** Liste (Rufzeichen, Referenz, Frequenz, Mode, Alter in Minuten), Filter aus den Einstellungen, Detailansicht mit Parkname, Region, Kommentar und Spotter. OE-Spots hervorgehoben.
4. **DX:** Liste (DX, Frequenz, Mode, DXCC, Alter), Filter, Detailansicht mit Kommentar und Spotter.
5. **Mesh:** Nodes und Repeater online groß, letzte öffentliche Nachrichten, Top-Repeater.
6. **News:** Liste der Titel, Detailansicht mit **QR-Code** der URL (`lv_qrcode`).
7. **Einstellungen:** aus Phase 06 in den Rahmen integriert.

Zusätzlich: **Boot-Splash** (Logo aus `assets/`, Version, „73 de <Rufzeichen>“, ca. 1,5 s oder bis WLAN steht).

## Qualität
- Speicher: LVGL-Heap im PSRAM, keine Allokationen im Render-Pfad pro Frame
- Flüssigkeit: Swipe ohne sichtbares Ruckeln, sonst Animationen reduzieren
- Host-Tests für die Rechenfunktionen (Sonnenzeiten, Locator → Lat/Lon, Bandzuordnung aus kHz, Altersformatierung)
- Screenshots aller Seiten über den LVGL-Snapshot per Serial-Kommando oder aus dem LVGL-PC-Simulator (SDL) nach `docs/screenshots/`

## Akzeptanzkriterien
- Alle Seiten mit Demo-Daten aus der Compose-Umgebung bedienbar, die Screenshots liegen im Repo
- Bedienung komplett ohne Touch möglich (Trackball und Tastatur)
- CI grün inkl. Host-Tests
