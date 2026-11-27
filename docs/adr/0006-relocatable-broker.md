# 0006 – Broker ist umziehbar

- **Status:** Accepted
- **Datum:** 2026-11-27

## Kontext
Das Homelab hängt an Starlink, Glasfaser kommt später. Für die Community ist ein Ausfall daheim ein Ausfall aller Geräte.

## Entscheidung
Geräte kennen nur den Hostnamen `mqtt.oeradio.at`. Phase 1: Broker und Aggregator im Homelab hinter dem Tunnel. Phase 2 (bei Bedarf): öffentlicher Broker auf einem kleinen VPS hinter Cloudflare, der Aggregator publiziert per Mosquitto-Bridge dorthin oder läuft ebenfalls am VPS.

## Alternativen
- Sofort VPS: unnötige Kosten in der Beta

## Konsequenzen
- Der Umzug ist eine reine Infrastruktur- und DNS-Änderung, die Firmware bleibt unverändert
- Retained Messages am VPS-Broker überbrücken Ausfälle daheim (Geräte zeigen Daten mit Alterskennzeichen)
