# oeradio-cockpit

**Ham-Radio-Cockpit für den LilyGO T-Deck – gebrandet für [oeradio.at](https://oeradio.at).**

Ein kleines Shack-Display, das neben dem Funkgerät steht und auf einen Blick zeigt, was los ist:
Bandzustand, POTA-Spots, DX-Cluster, MeshCore-Aktivität in Kärnten, oeradio.at-News und
UTC/Graulinie. Die Daten bereitet ein zentraler Aggregator auf und verteilt sie per MQTT über
WebSocket (WSS) an alle Geräte der Community.

> *English summary:* A community ham radio dashboard for the LilyGO T-Deck (ESP32-S3, WiFi).
> A central Python aggregator publishes solar/band, POTA, DX cluster, MeshCore and news data
> as retained MQTT messages; devices subscribe read-only via MQTT-over-WSS through Cloudflare.

## Status

🚧 **Planung / Aufbau (v0.x).** Siehe [`docs/architecture.md`](docs/architecture.md) und die
Build-Prompts unter [`prompts/`](prompts/).

## Komponenten

| Verzeichnis | Inhalt |
|---|---|
| `firmware/` | ESP-IDF-Firmware für den T-Deck v1 (LVGL 9, esp-mqtt über WSS) |
| `aggregator/` | Python-Dienst: holt Datenquellen, publiziert JSON nach MQTT |
| `schemas/` | Topic- und Nachrichten-Vertrag (JSON Schema, versioniert) |
| `deploy/` | Kustomize-Basis: Mosquitto, Aggregator, cloudflared (Referenz für GitOps) |
| `assets/` | Branding: Logo, Fonts, Icons (CC BY 4.0) |
| `web/` | Web-Flasher (ESP Web Tools) für `flash.oeradio.at` |
| `docs/` | Architektur, ADRs, Topic-Doku |

## Seiten (MVP)

1. **Uhr:** UTC und Lokalzeit, Graulinie, Sonnenauf- und -untergang (offline berechnet)
2. **Bandzustand:** SFI, K- und A-Index, Bandampel Tag/Nacht
3. **POTA:** Live-Spots, Filter OE und Nachbarländer, Detailansicht
4. **DX-Cluster:** Spots gefiltert nach Band und Mode
5. **Mesh:** CarinthiaMesh-Aktivität über den oeradio.at MeshCore Observer
6. **News:** neueste oeradio.at-Artikel
7. **Einstellungen:** Rufzeichen, Locator, mehrere WLANs, Filter

## Hardware

- LilyGO **T-Deck v1** (ESP32-S3, 16 MB Flash, 8 MB PSRAM, 2,8" 320×240 Touch, Tastatur, Trackball)
- Nur WLAN, kein LoRa erforderlich

## Mitmachen

Siehe [`CONTRIBUTING.md`](CONTRIBUTING.md). Sicherheitsprobleme bitte wie in [`SECURITY.md`](SECURITY.md) melden.

## Lizenz

- Code: [MIT](LICENSE)
- Branding-Assets unter `assets/` (Logo, Grafiken): [CC BY 4.0](LICENSE-ASSETS.md)
- Das Logo und der Name „oeradio.at“ sind Kennzeichen von oeradio.at. Forks bitte mit eigenem Branding.

73 de OE8YML
