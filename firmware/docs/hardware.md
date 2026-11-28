# Hardware: LilyGO T-Deck v1

Quelle: [Xinyuan-LilyGO/T-Deck](https://github.com/Xinyuan-LilyGO/T-Deck), Commit
[`9aac147`](https://github.com/Xinyuan-LilyGO/T-Deck/tree/9aac147a313975ad063594b3fe11e28a96d94936)
(9aac147a313975ad063594b3fe11e28a96d94936), gelesen am 2026-11-27.
**Aus dem Code übernommen, noch nicht auf einem Gerät geprüft.**

## Pins

| Funktion | GPIO | Quelle |
|---|---|---|
| Peripherie-Versorgung (`BOARD_POWERON`, zuerst auf HIGH) | 10 | `examples/UnitTest/utilities.h` |
| I²C SDA / SCL | 18 / 8 | `utilities.h` |
| SPI MOSI / MISO / SCK | 41 / 38 / 40 | `utilities.h` |
| Display CS / DC | 12 / 11 | `utilities.h` |
| Display Reset | nicht verdrahtet | `lib/TFT_eSPI/User_Setups/Setup210_LilyGo_T_Deck.h` |
| Display-Backlight | 42 | `utilities.h` |
| SD-Karte CS / Radio CS (bei Nichtnutzung HIGH) | 39 / 9 | `utilities.h` |
| Touch INT | 16 | `utilities.h` |
| Trackball rechts / oben / links / unten | 2 / 3 / 1 / 15 | `utilities.h`, Richtungen aus `UnitTest.ino` |
| Trackball-Klick (zugleich BOOT) | 0 | `utilities.h` |
| Akku-ADC | 4 | `utilities.h` |

## Bausteine

- **Display:** ST7789, 240×320, SPI mit 40 MHz, quer genutzt. `TFT_eSPI` mit Rotation 1 setzt MADCTL `MX | MV`,
  Farbreihenfolge RGB und Farbinversion ein (`TFT_Drivers/ST7789_Init.h`, `ST7789_Rotation.h`).
- **Backlight:** **kein PWM.** Ein Treiber mit 16 Stufen, die per kurzer Low-Impulse gewählt werden
  (`setBrightness()` in `UnitTest.ino`). Länger als 3 ms LOW schaltet aus. Deshalb kein LEDC.
- **Touch:** GT911, I²C-Adresse 0x5D oder 0x14 (wird beim Start abgefragt). LilyGO: `setSwapXY(true)`,
  `setMirrorXY(false, true)`. `esp_lcd_touch` spiegelt **vor** dem Tauschen, daher dort `swap_xy` und `mirror_x`.
- **Tastatur:** eigener ESP32-C3, I²C-Adresse 0x55. Ein gelesenes Byte ist das Zeichen, 0 = nichts.
  Enter `0x0D`, Backspace `0x08`. Backlight: `0x01 <0–255>` (`examples/Keyboard_ESP32C3`, `Keyboard_T_Deck_Master`).
  Ältere Tastatur-Firmware kennt das Backlight-Kommando eventuell nicht.
- **Akku:** ADC-Spannung × 2 (`analogReadMilliVolts(BOARD_BAT_ADC) * 2` in `UnitTest/ui.cpp`).
  Die Prozentanzeige ist eine grobe LiPo-Kurve. Am USB-Kabel wird die Ladespannung gemessen.

## Boot-Modus

Trackball-Mitte gedrückt halten und dann USB anstecken (README des LilyGO-Repos, „Quick start“).
