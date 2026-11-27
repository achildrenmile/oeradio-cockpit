# Schemas v1

Hier liegen die JSON Schemas der MQTT-Nachrichten, also der Vertrag zwischen Aggregator und Firmware.
Sie werden aus den pydantic-Modellen des Aggregators generiert und nicht von Hand bearbeitet.
Neu erzeugen: `uv run python -m cockpit schemas export` im Verzeichnis `aggregator/`.
Die Erklärung der Felder steht in [`docs/mqtt-topics.md`](../../docs/mqtt-topics.md).
