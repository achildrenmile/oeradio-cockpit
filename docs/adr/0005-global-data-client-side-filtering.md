# 0005 – Globale Daten, Filterung im Gerät

- **Status:** Accepted
- **Datum:** 2026-11-27

## Kontext
Jeder OM will andere Bänder, Modes und Regionen sehen. Filtern könnte man pro Gerät am Server (eigene Topics) oder im Gerät.

## Entscheidung
Der Aggregator publiziert einen globalen, vorsortierten und gekürzten Datensatz pro Topic. Die Filter liegen in den Geräteeinstellungen (NVS) und werden in der Firmware angewendet.

## Alternativen
- Topics pro Gerät oder Rufzeichen: braucht Registrierung und Server-State und skaliert linear mit der Gerätezahl

## Konsequenzen
- Kein Server-State, keine Nutzerverwaltung, ein Broker reicht für sehr viele Geräte
- Payloads müssen klein bleiben (vgl. `max` in `docs/mqtt-topics.md`)
