# Firmware

ESP-IDF-Firmware für den LilyGO T-Deck v1 mit LVGL 9 (siehe [ADR 0004](../docs/adr/0004-esp-idf-lvgl-firmware.md)).
Stand: Board-Bring-up mit Testseite. Netz und MQTT folgen in [Phase 06](../prompts/06-firmware-connectivity.md),
die Seiten in [Phase 07](../prompts/07-firmware-ui.md). Pins und Quellen: [`docs/hardware.md`](docs/hardware.md).

## Aufbau

| Verzeichnis | Inhalt |
|---|---|
| `components/board/` | Display, Touch, Tastatur, Trackball, Backlight, Akku |
| `main/ui/pages/` | Seiten (derzeit nur der Hardwaretest) |
| `test_host/` | Tests der hardwarefreien Rechenfunktionen |

Binärgröße (Stand Phase 05): App ca. 542 KiB, belegt 13 % eines OTA-Slots (4 MiB).

## Bauen

Mit installiertem [ESP-IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32s3/get-started/):

```sh
cd firmware
idf.py set-target esp32s3
idf.py build
```

Ohne Installation mit Docker (Image ca. 12 GB; die Build-Dateien gehören danach `root`):

```sh
docker run --rm -v "$PWD:/project" -w /project espressif/idf:release-v5.5 idf.py set-target esp32s3 build
```

Host-Tests:

```sh
cc -std=c11 -Wall -Wextra -Werror -Icomponents/board test_host/test_board_math.c components/board/board_math.c -o /tmp/board_math && /tmp/board_math
```

## Flashen

1. T-Deck in den Boot-Modus bringen: **Trackball gedrückt halten und USB anstecken.**
2. Flashen und mitlesen (Port anpassen, unter Linux meist `/dev/ttyACM0`):

```sh
idf.py -p /dev/ttyACM0 flash monitor
```

Danach einmal die Reset-Taste drücken oder neu anstecken, falls das Gerät im Boot-Modus bleibt.

Mit den Binärdateien aus der CI (Artefakt `firmware`) und [`esptool`](https://github.com/espressif/esptool):

```sh
esptool.py --chip esp32s3 -p /dev/ttyACM0 write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin \
  0xf000 ota_data_initial.bin 0x20000 oeradio-cockpit.bin
```

## Prüfliste am Gerät

- [ ] Testseite sichtbar, richtig herum, nicht gespiegelt, Farben plausibel (dunkler Text auf hellem Grund)
- [ ] Touch: Koordinaten oben links nahe 0,0, unten rechts nahe 319,239
- [ ] Tastatur: Zeichen erscheinen im Textfeld und bei „Taste“, Backspace löscht
- [ ] Trackball: jede Richtung zählt den richtigen Zähler, Klick zählt einmal pro Druck
- [ ] Trackball bewegt den Fokus zwischen Textfeld, Regler und Schalter
- [ ] Helligkeitsregler dimmt stufenweise ohne Flackern oder Ausfall
- [ ] Schalter „Tastenlicht“ schaltet die Tastaturbeleuchtung
- [ ] Akkuspannung plausibel (ca. 3,5–4,2 V ohne USB)
- [ ] Keine Fehlermeldungen oder Neustarts im Monitor
