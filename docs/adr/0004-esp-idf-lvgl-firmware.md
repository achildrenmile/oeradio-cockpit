# 0004 – Firmware mit ESP-IDF und LVGL 9

- **Status:** Accepted
- **Datum:** 2026-11-27

## Kontext
Der T-Deck v1 (ESP32-S3) soll eine verteilbare, OTA-fähige Firmware mit Touch, Tastatur und Trackball bekommen. Die Optionen waren ESPHome+LVGL, Arduino/PlatformIO und natives ESP-IDF.

## Entscheidung
Natives ESP-IDF 5.x mit `esp_lvgl_port` und LVGL 9, Abhängigkeiten über die ESP Component Registry, CI über die offiziellen ESP-IDF-Container bzw. `espressif/esp-idf-ci-action`.

## Alternativen
- ESPHome: schnell, aber eigene Seitenlogik, Tastatur und Trackball sowie das Branding sind mühsam. Für ein Community-Produkt zu unflexibel.
- Arduino/PlatformIO: Die offizielle PlatformIO-Plattform hinkt bei Arduino-Core 3.x hinterher (Fork nötig). WSS-MQTT und OTA laufen ohnehin über IDF-Komponenten.

## Konsequenzen
- `esp-mqtt` (WSS), `esp_crt_bundle`, `esp_https_ota` und NVS sind native Bausteine
- Etwas mehr Boilerplate bei der Hardware-Initialisierung. Pins kommen aus dem LilyGO-Repo und sind zu verifizieren.
