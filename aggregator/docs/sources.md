# Datenquellen

Stand: 2026-11-27. Alle HTTP-Abfragen senden `User-Agent: oeradio-cockpit/<version> (+https://oeradio.at)`,
haben 20 s Timeout und nach Fehlern ein wachsendes Intervall (nie kürzer als das reguläre).

## solar – N0NBH / hamqsl.com

- **URL:** `https://www.hamqsl.com/solarxml.php` (XML, mit `defusedxml` gelesen)
- **Intervall:** 3600 s. Die [Seite](https://www.hamqsl.com/solar.html) bittet bei den Feeds um höchstens stündliche Updates.
- **Nutzung:** keine formale Lizenz; Quellenangabe „HAMQSL.com“ ist erwünscht. **Offen:** Die Firmware soll sie auf der Bandseite zeigen.
- Fehlt der solare Flux, wird nichts publiziert. Andere fehlende Werte entfallen in der Nachricht.

## pota – Parks on the Air

- **URL:** `https://api.pota.app/spot/activator` (JSON, Liste der aktiven Spots)
- **Intervall:** 90 s
- **Nutzung:** keine veröffentlichten API-Bedingungen oder Limits gefunden (docs.pota.app). Die API ist nicht offiziell dokumentiert und kann sich ändern. Kontakt: help@parksontheair.com.
- Europa über die Programmpräfixe (`COCKPIT_POTA_EUROPE`), ohne Russland. Als ungültig markierte Spots und Frequenzen außerhalb der Amateurbänder entfallen.

## dx – DX-Cluster

- **Zugang:** Telnet, Default `dxspider.co.uk:7300`. Anmeldung mit einem Rufzeichen (`COCKPIT_DX_CALLSIGN`, kein Default im Code).
- **Format:** Zeilen `DX de <Spotter>: <kHz> <DX> <Kommentar> <HHMM>Z`
- **Verhalten:** Dedupe je DX-Station und Band für 5 min, Publikation alle 15 s, Spots älter als 60 min entfallen, Neuverbindung mit Backoff (5 s bis 10 min) und nach 10 min Stille.
- **Mode:** geraten aus Kommentar und Frequenz; im Zweifel fehlt `m`.
- **Nutzung:** Betrieb durch Freiwillige; nur eine Verbindung, es wird nichts gesendet außer dem Rufzeichen.
- Ein zweiter Login mit demselben Rufzeichen kann die erste Sitzung trennen. Für den Dauerbetrieb eine eigene SSID verwenden.

### DXCC-Tabelle

`src/cockpit/data/cty.dat` von Jim Reisert, AD1C (<https://www.country-files.com/>), Version `VER20261126`.
Lizenz im MIT-Stil, siehe [`LICENSE-cty.txt`](../src/cockpit/data/LICENSE-cty.txt). Aktualisieren: Datei ersetzen und Version hier nachtragen.

## news – oeradio.at

- **URL:** `https://oeradio.at/feed/` (RSS 2.0, mit `feedparser`; auch Atom möglich). Ein JSON-Feed wird nicht unterstützt.
- **Intervall:** 1800 s, mit `ETag`/`If-None-Match`
- **Nutzung:** eigene Inhalte; publiziert werden nur Titel, Datum und Link.

## mesh – MeshCore Observer

- **Zugang:** MQTT-Subscribe auf den Observer-Broker (`COCKPIT_MESH_*`), Default-Topic `meshcore/+/+/packets`. Es wird nie dorthin publiziert.
- **Format:** JSON im Stil von `meshcore-packet-capture`. Abgeleitet aus [meshfeed](https://github.com/achildrenmile/meshfeed) (Unlicense),
  **noch nicht gegen den laufenden Observer geprüft** (TODO). Ein anderes Format braucht nur einen neuen `ObserverAdapter`.
- **Datenschutz:** Entschlüsselt werden nur `public` und Hashtag-Kanäle aus `COCKPIT_MESH_CHANNELS`. Private Schlüssel lassen sich nicht konfigurieren, Direktnachrichten sind nicht lesbar.
- **Zählung:** Nodes und Repeater mit verifiziertem Advert innerhalb von `COCKPIT_MESH_WINDOW_HOURS` (Default 24). Der Zustand liegt im Speicher und beginnt nach einem Neustart bei null.
- Bei unbekanntem Format steht einmalig `mesh_unknown_format` im Log. Retained Pakete werden ignoriert.
