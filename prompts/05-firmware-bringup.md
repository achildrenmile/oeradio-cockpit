# Phase 05 – Firmware: Board-Bring-up (T-Deck v1)

Lies zuerst `CLAUDE.md`, `docs/architecture.md` und ADR 0004.

## Ziel
Ein ESP-IDF-Projekt in `firmware/`, das auf dem LilyGO T-Deck v1 Display, Touch, Tastatur, Trackball,
Backlight und Akkuspannung zum Laufen bringt und mit LVGL 9 eine Testseite zeigt.

## Wichtig: Hardware verifizieren
Hole die Pinbelegung und die Init-Sequenzen aus dem **offiziellen LilyGO-Repo** (`Xinyuan-LilyGO/T-Deck`,
v. a. `examples/*/utilities.h` und die Beispiele für Tastatur und Touch) und dokumentiere die Quelle mit Commit-Hash in
`firmware/docs/hardware.md`. Insbesondere zu prüfen:
- Board-Power-Enable-Pin (muss vor allen Peripheriegeräten gesetzt werden)
- Display-Controller (ST7789, SPI) inkl. Rotation bzw. Mirroring für Querformat 320×240, Backlight-Pin
- Touch-Controller (GT911, I²C) und Adresse, Koordinaten-Mapping zur Rotation
- Tastatur: eigener ESP32-C3 am I²C, liefert ASCII-Zeichen, I²C-Adresse prüfen, Tastatur-Backlight-Steuerung
- Trackball: 4 Richtungs-GPIOs (Impulse) und Klick
- Akku-ADC-Pin und Spannungsteiler
- Gemeinsamer SPI-Bus mit SD-Karte bzw. Radio (CS-Leitungen der nicht genutzten Geräte high setzen)

## Aufgaben
1. ESP-IDF-5.x-Projekt, Ziel `esp32s3`, `sdkconfig.defaults` (PSRAM octal an, Flash 16 MB, Partitionstabelle mit zwei OTA-Slots `ota_0`/`ota_1` + `nvs` + `otadata` + `spiffs`/`littlefs` für Assets).
2. `idf_component.yml`: `lvgl/lvgl` 9.x, `espressif/esp_lvgl_port`, `espressif/esp_lcd_touch_gt911`.
3. `components/board/`: `board_init()`, Display über `esp_lcd` + `esp_lvgl_port` (DMA, Double-Buffer im PSRAM), Touch als LVGL-Pointer, **Tastatur als LVGL-Keypad-Indev**, **Trackball als LVGL-Encoder-Indev** (Interrupt-basiertes Zählen mit Entprellung), Backlight per LEDC (0–100 %), `board_battery_mv()` und Prozentschätzung.
4. `main/`: Testseite mit Text, Touch-Koordinaten, letzter Taste, Trackball-Zählern und Akkuspannung.
   Eine LVGL-Gruppe, damit Trackball und Tastatur Widgets fokussieren können.
5. CI: Der Firmware-Job aus Phase 01 baut jetzt wirklich (`idf.py build`) und lädt `build/*.bin` als Artefakt hoch.
6. `firmware/README.md`: Toolchain installieren, bauen, flashen (`idf.py -p <port> flash monitor`), Boot-Modus des T-Deck (Trackball gedrückt halten beim Einstecken bzw. Reset, verifizieren).

## Akzeptanzkriterien
- CI-Build grün, Binary-Größe ist im README dokumentiert
- Auf echter Hardware (Michael testet): Testseite sichtbar, Touch-Koordinaten stimmen mit der Rotation überein, Tasten erscheinen, Trackball zählt, Akkuspannung plausibel
- `firmware/docs/hardware.md` mit Pin-Tabelle und Quellen-Link

## Nicht-Ziele
Kein WLAN und kein MQTT (Phase 06), kein finales UI (Phase 07).
