# 0003 – Öffentlicher Broker nur lesend, kein Access-Gate für Geräte

- **Status:** Accepted
- **Datum:** 2026-11-27

## Kontext
Cloudflare Zero Trust (Access) kann Hostnamen schützen. Community-Geräte müssten dafür ein gemeinsames Service-Token in der öffentlichen Firmware tragen.

## Entscheidung
Der Geräte-Hostname bekommt keine Access-Policy. Mosquitto erlaubt Geräten nur `read oeradio/cockpit/#` (anonym oder mit gemeinsamem Read-only-User). Schreiben darf nur der Aggregator über einen cluster-internen Listener, der nicht im Tunnel liegt. Davor stehen Cloudflare WAF und Rate Limiting sowie `max_connections` in Mosquitto. Admin-Oberflächen liegen hinter Access mit Login.

## Alternativen
- Gemeinsames Service-Token: kein echter Schutz, weil öffentlich auslesbar, aber zusätzliche Komplexität
- Gerätespezifische Credentials ab Tag 1: braucht Registrierungsprozess und Nutzerverwaltung, zu viel für das MVP

## Konsequenzen
- Es werden ausschließlich öffentliche Daten verteilt. Keine Direktnachrichten, keine personenbezogenen Daten über die Spot-Rufzeichen hinaus.
- Option für später: Registrierung über oeradio.at, die einzeln sperrbare Credentials pro Gerät ausgibt (`mosquitto-go-auth` o. ä.)
