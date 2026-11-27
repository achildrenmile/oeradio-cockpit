# Phase 04 – Broker, Container, Deploy

Lies zuerst `CLAUDE.md`, `docs/architecture.md` sowie ADR 0001, 0003 und 0006.

## Ziel
Container-Image für den Aggregator, eine gehärtete Mosquitto-Konfiguration und eine Kustomize-Basis,
die Michaels privates GitOps-Repo (ArgoCD, k3s) referenzieren kann.

## Aufgaben
1. `aggregator/Dockerfile`: Multi-Stage mit `uv`, Laufzeit `python:3.12-slim`, Non-Root-User, read-only Root-FS-fähig, Healthcheck über eine Datei bzw. einen Prozess-Check. Multi-Arch (amd64, arm64).
2. Workflow `.github/workflows/aggregator-image.yml`: Build und Push nach `ghcr.io/<owner>/oeradio-cockpit-aggregator` bei Tags `aggregator-v*` sowie `:edge` auf `main`. SBOM und Provenance aktiv.
3. `deploy/mosquitto/mosquitto.conf`:
   - Listener `1883` (nur cluster-intern), Auth per Passwortdatei, User `aggregator` mit Schreibrecht
   - Listener `9001` mit `protocol websockets` (Ziel des Tunnels)
   - Geräte: anonym **oder** gemeinsamer User `device` (beides vorbereiten, per Kommentar umschaltbar), ACL nur `read oeradio/cockpit/#`
   - `persistence true` für Retained Messages, `max_connections`, `message_size_limit 8192`, `max_inflight_messages`, `allow_zero_length_clientid false` prüfen
   - `deploy/mosquitto/acl.example` und `passwd.example` (nur Struktur, keine echten Hashes)
4. `deploy/base/` (Kustomize): Namespace `oeradio-cockpit`, Mosquitto (StatefulSet mit PVC, Services `mqtt-internal:1883` und `mqtt-ws:9001`), Aggregator (Deployment, ConfigMap für `COCKPIT_*`, Secret-Referenzen), cloudflared (Deployment mit 2 Replicas, Token aus Secret, Ingress-Regel `mqtt.oeradio.at → http://mqtt-ws:9001`), NetworkPolicies (nur cloudflared → 9001, nur Aggregator → 1883).
   Alle Secrets nur als `*.example.yaml` mit Kommentar, wie Michael sie in seinem GitOps-Repo (SOPS/Sealed Secrets) erstellt.
5. `deploy/README.md`: Einbindung als Remote-Base in ein GitOps-Repo, benötigte Secrets, Cloudflare-Seite (Tunnel-Hostname, WAF/Rate-Limit-Empfehlung, Access nur für Admin-Hostnamen), Hinweis auf Keepalive ≤ 60 s und Starlink/CGNAT (Tunnel ist nur ausgehend).
6. `deploy/compose/docker-compose.yml` für lokale Tests: Mosquitto und Aggregator (Demo-Quelle), Mosquitto-WS auf `localhost:9001`.
7. Test-Skript `deploy/scripts/ws-smoke.py`: verbindet sich per WSS bzw. WS (Parameter), abonniert `oeradio/cockpit/#`, prüft, dass alle Topics innerhalb von 5 s retained ankommen, und prüft, dass Publizieren als Gerät **abgelehnt** wird.

## Akzeptanzkriterien
- `docker compose up` lokal → `ws-smoke.py ws://localhost:9001` grün (Lesen ok, Schreiben verweigert)
- `kustomize build deploy/base` läuft fehlerfrei, `kubeconform` validiert
- Image-Workflow baut erfolgreich (ohne Push im PR)

## Nicht-Ziele
Kein produktives Deployment und keine echten Secrets. Das macht Michael im GitOps-Repo.
