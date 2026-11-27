# Deploy

Referenz für Mosquitto, Aggregator und cloudflared. Das produktive Deployment liegt in einem privaten
GitOps-Repo, das diese Basis referenziert. Hintergrund: ADR [0001](../docs/adr/0001-mqtt-over-wss-via-cloudflare-tunnel.md),
[0003](../docs/adr/0003-read-only-public-broker.md), [0006](../docs/adr/0006-relocatable-broker.md).

| Verzeichnis | Inhalt |
|---|---|
| `mosquitto/` | Broker-Konfiguration und ACLs |
| `base/` | Kustomize-Basis (Namespace `oeradio-cockpit`) |
| `compose/` | lokaler Test mit Demo-Daten |
| `scripts/ws-smoke.py` | prüft die Geräteseite des Brokers |

## Lokal testen

```sh
docker compose -f deploy/compose/docker-compose.yml up -d --build
MQTT_WRITER_PASSWORD=local-test-only uv run deploy/scripts/ws-smoke.py ws://localhost:9001
docker compose -f deploy/compose/docker-compose.yml down -v
```

## Broker

| Listener | Zugriff | Auth |
|---|---|---|
| 1883 | nur Cluster-intern, User `aggregator` schreibt | Passwortdatei |
| 9001 (WebSocket) | Geräte, **nur lesen** | anonym (Default) oder User `device` |

Die Auth ist pro Listener gebunden (`plugin_use`). Die globalen Optionen `password_file`/`acl_file`
würden den Schreib-Login auch am öffentlichen Listener gültig machen. Braucht Mosquitto ≥ 2.1.

Geräte-Variante B (gemeinsamer User): in `mosquitto.conf` die markierten Zeilen umschalten und
`device.passwd` ins Secret `mosquitto-auth` aufnehmen.

Client-IDs gelten über alle Listener. Wer eine ID kennt, kann diesen Client trennen. Der Aggregator
nutzt deshalb eine zufällige ID, und die Firmware soll das ebenfalls tun.

## Ins GitOps-Repo einbinden

```yaml
apiVersion: kustomize.config.k8s.io/v1beta1
kind: Kustomization
resources:
  - https://github.com/achildrenmile/oeradio-cockpit//deploy/base?ref=<commit-oder-tag>
  - secrets.enc.yaml   # eigene, verschlüsselte Secrets
images:
  - name: ghcr.io/achildrenmile/oeradio-cockpit-aggregator
    newTag: "<version>"   # Default der Basis ist "edge"
```

Zusätzliche Quellen per Patch aktivieren, z. B.:

```yaml
configMapGenerator:
  - name: aggregator-config
    behavior: merge
    literals:
      - COCKPIT_SOURCES=solar,pota,dx,news,mesh
```

`dx` braucht `COCKPIT_DX_CALLSIGN`, `mesh` braucht `COCKPIT_MESH_HOST` (siehe
[`aggregator/README.md`](../aggregator/README.md)). Ohne diese Werte startet der Aggregator nicht.

## Secrets

Vorlagen in `base/secret-*.example.yaml`. Sie sind bewusst nicht Teil der Kustomization.

| Secret | Schlüssel |
|---|---|
| `mosquitto-auth` | `internal.passwd` (und `device.passwd` für Variante B), Hashes von `mosquitto_passwd` |
| `aggregator-secrets` | `COCKPIT_BROKER_PASSWORD`, optional DX- und Mesh-Werte |
| `cloudflared-token` | `token` |

Verschlüsselt mit SOPS oder als Sealed Secret ablegen, nie im Klartext committen.

## Cloudflare

1. Tunnel anlegen (Zero Trust → Networks → Tunnels) und das Token ins Secret übernehmen.
2. Public Hostname `mqtt.oeradio.at` → Service `http://mqtt-ws:9001`. Der Tunnel ist remote verwaltet,
   die Ingress-Regel liegt also bei Cloudflare und nicht im Repo.
3. WebSockets für die Zone aktiviert lassen.
4. WAF und Rate Limiting für den Hostnamen, z. B. höchstens 30 Verbindungsaufbauten pro Minute und IP.
   Die Geräte verbinden sich selten, aber nach Edge-Reloads gleichzeitig (Reconnect mit Jitter).
5. Cloudflare Access **nur** für Admin-Hostnamen, nie für `mqtt.oeradio.at` (ADR 0003).

## Betrieb

- **Keepalive ≤ 60 s:** Cloudflare trennt untätige WebSockets nach rund 100 s.
- **Starlink/CGNAT:** unproblematisch, der Tunnel baut nur ausgehende Verbindungen auf.
- Die NetworkPolicies wirken nur, wenn das CNI sie durchsetzt (k3s tut das standardmäßig).
- Der Broker hat ein PVC für Retained Messages. Der Aggregator ist zustandslos und läuft genau einmal.
- Image: `ghcr.io/achildrenmile/oeradio-cockpit-aggregator` (`:edge` von `main`, Versionen aus Tags `aggregator-v*`).
