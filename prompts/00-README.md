# Build-Prompts

Aufeinander aufbauende Prompts für Claude Code (oder einen anderen Coding-Agent). Jede Phase ist
ein eigener Branch und PR, dazwischen schaust du kurz drüber und mergst.

## Ablauf

```
git switch -c phase/01-bootstrap
claude   # Inhalt von prompts/01-bootstrap.md einfügen
# → review, merge, nächste Phase
```

Jeder Prompt setzt voraus, dass der Agent zuerst `CLAUDE.md`, `docs/architecture.md`,
`docs/mqtt-topics.md` und die ADRs liest. Das steht in jedem Prompt explizit drin.

## Reihenfolge und Abhängigkeiten

| # | Phase | hängt ab von | parallel möglich mit |
|---|---|---|---|
| 01 | Repo-Bootstrap und CI-Gerüst | – | – |
| 02 | Aggregator-Kern und Schemas | 01 | 05 |
| 03 | Aggregator-Datenquellen | 02 | 05, 06 |
| 04 | Broker, Container, Deploy | 02 | 05, 06 |
| 05 | Firmware: Board-Bring-up | 01 | 02–04 |
| 06 | Firmware: Netz, MQTT, Einstellungen | 05, 02 (Schemas) | 03, 04 |
| 07 | Firmware: Seiten (UI) | 06 | 08 |
| 08 | Branding-Assets | 01 | 05–07 |
| 09 | Release, Web-Flasher, OTA | 07, 04 | – |
| 10 | Härtung und Beta | alle | – |

Firmware (05–07) und Aggregator (02–04) können parallel in zwei Sessions laufen. Der
Treffpunkt ist der Vertrag in `schemas/`.

## Was du selbst erledigen musst (nicht der Agent)

- GitHub-Repo `oeradio-cockpit` anlegen (public) und pushen
- Cloudflare: Tunnel anlegen, Hostname `mqtt.oeradio.at` auf den Tunnel-Service zeigen lassen,
  WAF- und Rate-Limit-Regel, Access-App für Admin-Hostnamen, Pages-Projekt für `flash.oeradio.at`
- Secrets in deinem GitOps-Repo (Tunnel-Token, Mosquitto-Passwörter, OTA-Signing-Key)
- Logo in Vektorform nach `assets/source/` legen (Phase 08)
- Den T-Deck per USB anstecken und flashen. Der Agent kann bauen, aber nicht auf deine Hardware schauen.
