# 0001 – MQTT über WebSocket (WSS) via Cloudflare Tunnel

- **Status:** Accepted
- **Datum:** 2026-11-27

## Kontext
Die Geräte stehen bei Community-Mitgliedern im Internet. Das Homelab sitzt hinter Starlink-CGNAT (keine eingehenden Ports). Rohes MQTT/TLS auf 8883 lässt sich über Cloudflare nur mit Spectrum (kostenpflichtig) oder mit cloudflared auf dem Client durchreichen. Beides ist für einen ESP32 keine Option.

## Entscheidung
Mosquitto bietet einen WebSocket-Listener an, der über einen Cloudflare Tunnel (cloudflared im k3s, nur ausgehend) als `wss://mqtt.oeradio.at` veröffentlicht wird. TLS terminiert an der Cloudflare-Edge. Die Firmware nutzt `esp-mqtt` mit WSS-Transport und `esp_crt_bundle`.

## Alternativen
- Portfreigabe/DynDNS: unmöglich hinter CGNAT, zudem keine DDoS-Abwehr
- Cloudflare Spectrum: kostenpflichtig
- Öffentlicher MQTT-Hoster: Abhängigkeit, Kosten, weniger Kontrolle
- HTTP-Polling statt MQTT: mehr Last, keine Push-Updates

## Konsequenzen
- Keepalive ≤ 60 s nötig (Cloudflare-WebSocket-Idle-Timeout ca. 100 s)
- Reconnect mit Backoff und Jitter ist Pflicht (Edge-Reloads, Starlink-Aussetzer)
- Kein Zertifikats-Pinning möglich bzw. sinnvoll (Cloudflare rotiert Zertifikate und CAs)
- Jede Nachricht geht pro verbundenem Gerät einmal über den Upload des Homelabs. Bei kompakten Payloads unkritisch.
