# Architektur

## Überblick

```
 Datenquellen                    Homelab (k3s, hinter Starlink-CGNAT)            Cloudflare            Community
 ─────────────                   ─────────────────────────────────────            ──────────            ─────────
 hamqsl.com (Solar) ─┐
 api.pota.app ───────┤          ┌────────────┐  MQTT 1883   ┌────────────┐
 DX-Cluster (Telnet) ┼─────────▶│ aggregator │─────────────▶│ mosquitto  │
 oeradio.at Feed ────┤          └────────────┘   (intern,   │            │ WS 9001  ┌────────────┐  outbound   ┌──────┐  wss://mqtt.oeradio.at  ┌─────────┐
 MeshCore Observer ──┘                            write)    │            │◀─────────│ cloudflared│────────────▶│ Edge │◀───────────────────────│ T-Deck  │ × n
   (MQTT-Bridge)                                            └────────────┘          └────────────┘   Tunnel    └──────┘   read-only, retained  └─────────┘
```

## Prinzipien

1. **Das Gerät ist dumm, der Server klug.** Der T-Deck parst nur kompaktes JSON aus wenigen Topics.
   Er braucht keine API-Keys und kein XML- oder HTML-Parsing. Neue Datenquellen erfordern kein Neuflashen.
2. **Globale Daten, lokale Filter.** Der Aggregator publiziert OE-relevante Daten für alle. Die Filter
   pro Gerät (Bänder, Modes, Länder) wendet die Firmware an. Am Server gibt es keine Nutzerverwaltung (ADR 0005).
3. **Retained First.** Jedes Daten-Topic ist retained. Ein Gerät hat nach Boot oder Reconnect sofort
   den aktuellen Stand. Jede Nachricht trägt `ts` (Erzeugungszeit), die Firmware zeigt das Alter der Daten an.
4. **Offline-Grundfunktion.** Uhr, Graulinie und Sonnenzeiten berechnet die Firmware lokal aus Locator und NTP-Zeit.
5. **Broker umziehbar.** Geräte kennen nur `mqtt.oeradio.at`. Ob der Broker im Homelab oder auf
   einem VPS läuft, ist eine Konfigurationsfrage (ADR 0006).

## Netzwerk und Sicherheit

| Pfad | Schutz |
|---|---|
| Gerät → `wss://mqtt.oeradio.at` | TLS (Cloudflare-Edge), Cloudflare WAF und Rate Limiting, Mosquitto-ACL **read-only** auf `oeradio/cockpit/#` |
| Aggregator → Mosquitto | Cluster-intern (Listener 1883, nicht im Tunnel), eigener User mit Schreibrecht |
| Admin-UIs (Monitoring o. ä.) | Cloudflare Access (Zero Trust) mit Login |
| Firmware-Downloads `flash.oeradio.at` | Cloudflare Pages/R2, signierte OTA-Images |

**Warum keine Access-Policy für Geräte?** Ein gemeinsames Service-Token in öffentlicher Firmware ist
kein Geheimnis. Die Sicherheit kommt aus „nur lesen, nur öffentliche Daten“ und dem Rate Limiting.
Für gerätespezifische Credentials gibt es später eine Option (siehe ADR 0003).

### Cloudflare-Rahmenbedingungen
- WebSocket-Idle-Timeout ca. 100 s (nicht konfigurierbar) → MQTT-Keepalive **≤ 60 s**
- Edge-Reloads trennen lange Verbindungen → Reconnect mit exponentiellem Backoff und **Jitter**
- Starlink-CGNAT: unproblematisch, weil der Tunnel nur ausgehend arbeitet. Kurze Aussetzer werden
  durch Reconnect und Retained Messages abgefangen.

## Datenfluss pro Quelle (MVP)

| Quelle | Methode | Intervall | Topic |
|---|---|---|---|
| hamqsl.com Solar-XML | HTTP-Poll | 30 min | `oeradio/cockpit/v1/solar` |
| api.pota.app Spots | HTTP-Poll | 90 s | `oeradio/cockpit/v1/pota` |
| DX-Cluster | Telnet-Stream, gebündelt | 15 s | `oeradio/cockpit/v1/dx` |
| MeshCore Observer | MQTT-Subscribe → aggregiert | 30 s | `oeradio/cockpit/v1/mesh` |
| oeradio.at Feed | HTTP-Poll (RSS/JSON) | 30 min | `oeradio/cockpit/v1/news` |
| Aggregator selbst | – | 30 s | `oeradio/cockpit/v1/status` |

Details zu Feldern und Größen: [`mqtt-topics.md`](mqtt-topics.md).

## Ausbaustufen

- **v0.1 (MVP):** obige Seiten, Single-Broker im Homelab, Web-Flasher
- **v0.2:** OTA-Updates, Alarme (neues DXCC, POTA in der Nähe) über den Lautsprecher
- **v0.3:** Broker auf VPS, Aggregator per Bridge (ADR 0006)
- **später:** Rig-Status per CAT über rigctld/flrig im LAN, gerätespezifische Credentials
