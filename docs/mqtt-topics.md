# MQTT-Topic-Vertrag v1

Dieser Vertrag ist die Schnittstelle zwischen `aggregator/` und `firmware/`. Maßgeblich sind die
generierten JSON Schemas in `schemas/v1/`. Dieses Dokument erklärt sie.

## Konventionen

- Präfix: `oeradio/cockpit/v1/`. Ein Breaking Change bedeutet `v2/` parallel, `v1` läuft weiter, bis die Geräte migriert sind.
- Alle Daten-Topics: **QoS 1, retained**
- Payload: UTF-8-JSON, kompakt (keine Einrückung), **Ziel < 4 KB pro Nachricht** (Firmware-Puffer 8 KB hart)
- Pflichtfelder in jeder Nachricht:
  - `v` (int): Schema-Version des Topics
  - `ts` (int): Unix-Sekunden (UTC), wann der Aggregator die Nachricht erzeugt hat
  - `src_ts` (int, optional): Zeitstempel der Quelle, falls vorhanden
- Frequenzen in **kHz** (int), Zeiten als Unix-Sekunden, Bänder als String (`"20m"`, `"2m"`, `"70cm"`)
- Listen sind vom Aggregator vorsortiert und gekürzt (siehe `max`)

## Topics

### `oeradio/cockpit/v1/status`
```json
{"v":1,"ts":1764268800,"agg":"0.1.0","ok":{"solar":true,"pota":true,"dx":true,"mesh":false,"news":true},"motd":"Willkommen bei oeradio.at"}
```
`ok.<quelle>`: Die letzte Abfrage dieser Quelle war erfolgreich und die Daten sind nicht älter als 3 × Intervall.
`motd`: optionale Kurzmeldung (max. 80 Zeichen), wird in der Statusleiste bzw. auf der Startseite gezeigt.
Zusätzlich setzt der Aggregator ein **Last Will** `{"v":1,"ts":<connect-zeit>,"offline":true}` auf diesem Topic.

### `oeradio/cockpit/v1/solar`
```json
{"v":1,"ts":0,"src_ts":0,"sfi":142,"ssn":98,"a":8,"k":2,"xray":"B5.2","wind":412,
 "bands":[{"b":"80m-40m","day":"good","night":"good"},{"b":"30m-20m","day":"good","night":"fair"},
          {"b":"17m-15m","day":"fair","night":"poor"},{"b":"12m-10m","day":"poor","night":"poor"}],
 "vhf":{"aurora":false,"es_eu":"closed"}}
```
Zustände: `good | fair | poor`.

### `oeradio/cockpit/v1/pota`
```json
{"v":1,"ts":0,"max":40,"spots":[
 {"id":123,"t":1764268700,"act":"OE8YML/P","ref":"OE-0123","park":"Naturpark Dobratsch","loc":"AT-2",
  "f":14285,"b":"20m","m":"SSB","sp":"OE8XYZ","cmt":"QRT 15z"}]}
```
Der Aggregator filtert grob auf Europa und sortiert OE-Referenzen zuerst, danach DL, I, S5, HA, OK/OM, HB9.
Die Feinfilterung (Bänder/Modes) macht das Gerät.

### `oeradio/cockpit/v1/dx`
```json
{"v":1,"ts":0,"max":40,"spots":[
 {"t":1764268790,"dx":"VP8LP","f":14025,"b":"20m","m":"CW","de":"OE1ABC","cmt":"up 1","dxcc":"Falkland Is."}]}
```
Der Aggregator dedupliziert (gleiche DX/Band innerhalb von 5 min) und bündelt Updates alle 15 s.

### `oeradio/cockpit/v1/mesh`
```json
{"v":1,"ts":0,"nodes_online":37,"repeaters_online":9,
 "recent":[{"t":1764268600,"from":"OE8YML-Büro","ch":"Public","txt":"Test Dobratsch"}],
 "top_repeaters":[{"name":"Dobratsch","seen":1764268750}]}
```
`recent.txt` kürzt der Aggregator auf max. 80 Zeichen. Es kommen **nur öffentliche Kanäle** vor, niemals Direktnachrichten.

### `oeradio/cockpit/v1/news`
```json
{"v":1,"ts":0,"items":[{"t":1764200000,"title":"POTA-Wochenende im Gailtal","url":"https://oeradio.at/…"}]}
```
Max. 8 Einträge, `title` max. 90 Zeichen. Die Firmware zeigt die URL als QR-Code.

## Firmware-Verhalten bei fehlenden Daten

| Situation | Anzeige |
|---|---|
| `ts` älter als 3 × Intervall | Seite gedimmt, Hinweis „Stand HH:MM UTC“ |
| `status.offline == true` | Statusleiste: „Server offline“ |
| keine MQTT-Verbindung | Statusleiste: Wolken-Icon durchgestrichen, Reconnect-Countdown |
