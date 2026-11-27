# Phase 09 – Release, Web-Flasher, OTA

Lies zuerst `CLAUDE.md`, `docs/architecture.md` und den Firmware-Code.

## Ziel
Jede OM soll den T-Deck ohne Toolchain im Browser flashen können. Danach aktualisiert sich das
Gerät per OTA mit signierten Images.

## Aufgaben
1. **Versionierung:** SemVer, Git-Tags `firmware-vX.Y.Z`. Die Version kommt aus `git describe` in die Firmware und wird im Splash und in den Einstellungen angezeigt.
2. **Release-Workflow** `.github/workflows/firmware-release.yml` auf Tag: Build, Merge zu einem einzigen `oeradio-cockpit-tdeck-vX.Y.Z.bin` für den Erstflash (`esptool.py merge_bin`) plus App-Image für OTA, Checksums, GitHub-Release mit Changelog (aus Conventional Commits).
3. **Web-Flasher** in `web/`: statische Seite mit ESP Web Tools, Manifest pro Version, gebrandet (oeradio.at-Farben), Anleitung „Trackball halten, einstecken, Flashen klicken“ auf Deutsch, Link zu den Release Notes. Deployment-Workflow nach **Cloudflare Pages** (Projektname und API-Token als GitHub Secrets, im README dokumentiert) für `flash.oeradio.at`.
4. **OTA:**
   - Signierte App-Images (ESP-IDF Secure-Boot-unabhängige App-Signaturprüfung bzw. eigene Signatur mit Ed25519 über das Image; Ansatz wählen, als ADR `0007-ota-signing.md` dokumentieren). Der Private Key liegt nur als GitHub Secret vor, der Public Key ist in der Firmware.
   - Manifest `https://flash.oeradio.at/ota/stable.json` (Version, URL, SHA-256, Signatur, Mindestversion)
   - Firmware prüft beim Start und alle 24 h, zeigt „Update verfügbar“ in der Statusleiste und installiert erst nach Bestätigung (Taste `u`) mit Fortschrittsanzeige über `esp_https_ota`
   - **Rollback:** `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`. Die neue Firmware markiert sich erst als gültig, nachdem WLAN und MQTT einmal erfolgreich waren.
5. Kanäle `stable` und `beta`, wählbar in den Einstellungen.

## Akzeptanzkriterien
- Ein Test-Tag erzeugt ein Release mit beiden Binaries, die Pages-Seite flasht ein frisches Gerät erfolgreich (Michael testet)
- OTA von vX auf vX+1 klappt, ein manipuliertes Image wird abgelehnt, ein absichtlich kaputtes Image rollt zurück
- ADR 0007 vorhanden
