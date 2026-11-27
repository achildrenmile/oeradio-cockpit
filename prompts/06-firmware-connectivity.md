# Phase 06 – Firmware: Netz, MQTT, Einstellungen

Lies zuerst `CLAUDE.md`, `docs/architecture.md`, `docs/mqtt-topics.md`, `schemas/v1/*.schema.json` und ADR 0001, 0003 und 0005.

## Ziel
Der T-Deck verbindet sich zuverlässig mit einem von mehreren WLANs, holt die Zeit per NTP, abonniert
`wss://mqtt.oeradio.at` read-only und legt die geparsten Daten in einem thread-sicheren Store ab.
Einstellungen werden direkt am Gerät über die Tastatur gemacht.

## Aufgaben
1. `components/store/`
   - **Einstellungen in NVS:** Rufzeichen, Maidenhead-Locator (6-stellig, validiert), Zeitzone (POSIX-TZ, Default `CET-1CEST,M3.5.0,M10.5.0/3`), bis zu 5 WLANs (SSID und Passwort, Priorität), Broker-URI (Default `wss://mqtt.oeradio.at/mqtt`, Pfad gegen das Deployment aus Phase 04 prüfen), Filter (Bänder, Modes, Länderpräfixe für POTA/DX), Display-Helligkeit, Alarm an/aus
   - **Daten-Store:** ein Struct je Topic (aus den Schemas abgeleitet), Mutex-geschützt, Versionszähler pro Topic, damit das UI nur bei Änderungen neu zeichnet
2. `components/net/wifi`: Scan, Verbindung zum besten bekannten WLAN nach RSSI und Priorität, automatischer Wechsel bei Verbindungsverlust (Shack ↔ Handy-Hotspot), Events an das UI.
3. `components/net/time`: SNTP (`pool.ntp.org` und `at.pool.ntp.org`), Zeitzone aus den Einstellungen, Flag „Zeit gültig“.
4. `components/net/mqtt`: `esp-mqtt` mit `wss://`, `crt_bundle_attach = esp_crt_bundle_attach`, **Keepalive 45 s**, Client-ID `oec-<mac-suffix>`, Subscribe `oeradio/cockpit/v1/#` (QoS 1). Eigener Reconnect: exponentieller Backoff 2 s → 5 min mit **±30 % Jitter**. Puffer 8 KB, Nachrichten > 8 KB verwerfen und loggen.
5. JSON-Parsing mit `cJSON` (IDF-Komponente) in die Store-Structs. Unbekannte Felder ignorieren, fehlende Pflichtfelder → Nachricht verwerfen. Prüfung von `v`: Ist die Version höher als unterstützt, Hinweis „Firmware-Update empfohlen“.
6. Statusmodell für die UI-Statusleiste: WLAN (SSID, RSSI), MQTT (verbunden / Reconnect in N s), Server (`status.offline`), Zeit gültig, Akku.
7. **Einstellungsseite (LVGL)**, bedienbar mit Tastatur und Trackball ohne Touch-Zwang: Rufzeichen, Locator, WLAN-Liste (scannen, auswählen, Passwort tippen, löschen), Filter, Helligkeit. Speichern in NVS.
   **Erststart-Assistent:** Rufzeichen → Locator → WLAN, danach Wechsel zur Uhr-Seite.
8. Unit-Tests für die JSON-Parser auf dem Host (Unity im Linux-Target von ESP-IDF oder ein separates CMake-Host-Test-Target) mit den Beispiel-Payloads aus `docs/mqtt-topics.md`.

## Akzeptanzkriterien
- Host-Tests der Parser grün in der CI
- Auf Hardware gegen `docker compose` aus Phase 04 (`ws://<lan-ip>:9001`): Daten kommen im Store an (Log-Ausgabe), WLAN-Wechsel funktioniert, Reconnect nach einem Broker-Neustart mit Jitter sichtbar im Log
- Gegen `wss://mqtt.oeradio.at`, sobald der Tunnel steht: Verbindung bleibt > 30 min stabil (kein 100-s-Abriss)

## Nicht-Ziele
Keine Datenseiten (Phase 07), kein OTA (Phase 09).
