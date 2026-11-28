# Firmware

ESP-IDF-Firmware für den LilyGO T-Deck v1 mit LVGL 9 (siehe [ADR 0004](../docs/adr/0004-esp-idf-lvgl-firmware.md)).
Stand: WLAN, Zeit, MQTT, Datenspeicher und Einstellungen am Gerät. Die Datenseiten folgen in
[Phase 07](../prompts/07-firmware-ui.md). Pins und Quellen: [`docs/hardware.md`](docs/hardware.md).

## Aufbau

| Verzeichnis | Inhalt |
|---|---|
| `components/board/` | Display, Touch, Tastatur, Trackball, Backlight, Akku |
| `components/store/` | Einstellungen (NVS), JSON-Parser, thread-sicherer Datenspeicher |
| `components/net/` | WLAN, SNTP, MQTT |
| `main/ui/` | Statusleiste und Seiten |
| `test_host/` | Tests der hardwarefreien Teile |

Binärgröße (Stand Phase 06): App ca. 1,34 MiB, belegt 34 % eines OTA-Slots (4 MiB).

## Bedienung

Trackball bewegt den Fokus, Klick wählt aus, die Tastatur schreibt ins markierte Feld, Enter bestätigt.
Touch geht überall, ist aber nicht nötig. Beim Helligkeitsregler: klicken, rollen, zum Verlassen lang drücken.

Beim ersten Start fragt ein Assistent Rufzeichen, Locator und WLAN ab. Alles Weitere unter
„Einstellungen“: WLAN, Filter, Anzeige und Alarm, Erweitert (Zeitzone, Broker), Hardwaretest.
Filter und Alarm werden gespeichert, wirken aber erst ab Phase 07 bzw. v0.2.

## Netz

- **WLAN:** bis zu 5 Netze. Gewählt wird nach Signalstärke und Reihenfolge der Liste (ein Platz weiter
  oben zählt wie 10 dB). Gewechselt wird erst bei Verbindungsverlust. Versteckte SSIDs werden nicht
  unterstützt. Ein Netz mit Passwort wird nie unverschlüsselt verbunden.
- **Zeit:** SNTP über `pool.ntp.org` und `at.pool.ntp.org`.
- **MQTT:** Default `wss://mqtt.oeradio.at/mqtt`, nur lesend, Keepalive 45 s. Reconnect mit
  Backoff 2 s bis 5 min und ±30 % Jitter. Nachrichten über 8 KB werden verworfen.
- **Client-ID:** `oec-<MAC-Ende>-<Zufall>`. Der Zufallsteil verhindert, dass Fremde die ID erraten
  und das Gerät trennen.

WLAN-Passwörter liegen **unverschlüsselt** im NVS. Wer das Gerät in der Hand hat, kann sie auslesen.
`idf.py erase-flash` löscht alle Einstellungen.

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

Host-Tests (Parser, Validierung, WLAN-Auswahl, Backoff; mit AddressSanitizer). Sie brauchen CMake
und cJSON aus ESP-IDF (`IDF_PATH`) oder `-DCJSON_DIR=…`:

```sh
cmake -S test_host -B /tmp/host && cmake --build /tmp/host && ctest --test-dir /tmp/host --output-on-failure
```

Die Beispiel-Payloads liest der Test direkt aus [`docs/mqtt-topics.md`](../docs/mqtt-topics.md).

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

- [ ] Hardwaretest (im Assistenten oder unter Einstellungen) sichtbar, richtig herum, nicht gespiegelt, Farben plausibel (dunkler Text auf hellem Grund)
- [ ] Touch: Koordinaten oben links nahe 0,0, unten rechts nahe 319,239
- [ ] Tastatur: Zeichen erscheinen im Textfeld und bei „Taste“, Backspace löscht
- [ ] Trackball: jede Richtung zählt den richtigen Zähler, Klick zählt einmal pro Druck
- [ ] Trackball bewegt den Fokus zwischen Textfeld, Regler, Schalter und „Zurueck“
- [ ] Helligkeitsregler dimmt stufenweise ohne Flackern oder Ausfall
- [ ] Schalter „Tastenlicht“ schaltet die Tastaturbeleuchtung
- [ ] Akkuspannung plausibel (ca. 3,5–4,2 V ohne USB)
- [ ] Keine Fehlermeldungen oder Neustarts im Monitor

### Netz und Einstellungen

Gegen den lokalen Broker testen: siehe [`deploy/README.md`](../deploy/README.md#lokal-testen).

- [ ] Assistent: Rufzeichen → Locator → WLAN → Uhr, nur mit Tastatur und Trackball
- [ ] Statusleiste zeigt WLAN mit RSSI, MQTT, UTC-Zeit und Akku
- [ ] Monitor zeigt für jedes Topic eine Zeile `store: <topic>: …`
- [ ] Erstes WLAN abschalten: Gerät wechselt ins zweite gespeicherte Netz
- [ ] Broker neu starten: „reconnect in … ms“ mit wachsenden, gestreuten Abständen, danach wieder Daten
- [ ] Broker stoppen: Statusleiste zeigt Countdown; Aggregator stoppen: „Server offline“
- [ ] Einstellungen überstehen einen Neustart
- [ ] Keine Neustarts, keine Bildstörungen während WLAN und TLS aktiv sind
- [ ] Gegen `wss://mqtt.oeradio.at` (sobald der Tunnel steht): über 30 min stabil
