# Aggregator

Hier entsteht der Python-Dienst `cockpit`, der die Datenquellen abfragt und kompaktes JSON per MQTT publiziert (siehe [ADR 0002](../docs/adr/0002-python-aggregator.md)).
Der Kern mit Konfiguration, Publisher, Datenmodellen und Demo-Quelle folgt in [Phase 02](../prompts/02-aggregator-core.md).
Die echten Datenquellen kommen in [Phase 03](../prompts/03-aggregator-sources.md), das Container-Image in [Phase 04](../prompts/04-broker-deploy.md).
