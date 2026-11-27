#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["paho-mqtt>=2.1,<3"]
# ///
"""Smoke test of the device side of the broker.

Usage: uv run deploy/scripts/ws-smoke.py ws://localhost:9001 | wss://mqtt.oeradio.at

Checks that all topics arrive retained within 5 s and that publishing is rejected.
For the shared device user set MQTT_USER and MQTT_PASSWORD.
With MQTT_WRITER_PASSWORD set it also checks that the aggregator's login cannot write here.
"""

import argparse
import os
import sys
import time
from urllib.parse import urlsplit

import paho.mqtt.client as mqtt

PREFIX = "oeradio/cockpit/v1"
TOPICS = {f"{PREFIX}/{name}" for name in ("status", "solar", "pota", "dx", "mesh", "news")}
TIMEOUT = 5.0


def wait_for(condition, timeout: float = TIMEOUT) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.05)
    return bool(condition())


def connect(url: str, login: tuple[str, str] | None = None, required: bool = True) -> mqtt.Client:
    parts = urlsplit(url)
    if parts.scheme not in ("ws", "wss") or not parts.hostname:
        sys.exit("URL must start with ws:// or wss://")
    # MQTT 5, because only then the broker says why a publish was refused.
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2, transport="websockets", protocol=mqtt.MQTTv5
    )
    client.ws_set_options(path=parts.path or "/mqtt")
    if parts.scheme == "wss":
        client.tls_set()
    if login:
        client.username_pw_set(*login)
    elif user := os.environ.get("MQTT_USER"):
        client.username_pw_set(user, os.environ.get("MQTT_PASSWORD"))
    client.connect(parts.hostname, parts.port or (443 if parts.scheme == "wss" else 80), 60)
    client.loop_start()
    if not wait_for(client.is_connected) and required:
        sys.exit("FAIL: no connection")
    return client


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url")
    url = parser.parse_args().url

    first: dict[str, bool] = {}
    acks: dict[int, int] = {}
    probe = f"{PREFIX}/_smoke"

    reader = connect(url)
    reader.on_message = lambda c, u, m: first.setdefault(m.topic, bool(m.retain))
    # "#" on purpose: nothing outside the cockpit topics may show up.
    reader.subscribe("#", qos=1)
    wait_for(lambda: first.keys() >= TOPICS)

    failures = []
    if missing := sorted(TOPICS - first.keys()):
        failures.append(f"missing topics: {', '.join(missing)}")
    if live := sorted(t for t in TOPICS & first.keys() if not first[t]):
        failures.append(f"not retained: {', '.join(live)}")
    if foreign := sorted(t for t in first if not t.startswith("oeradio/cockpit/")):
        failures.append(f"readable outside the cockpit topics: {', '.join(foreign)}")

    writers = {"device": connect(url)}
    if password := os.environ.get("MQTT_WRITER_PASSWORD"):
        # May fail to connect, which is just as good as a refused publish.
        writers["aggregator login"] = connect(url, ("aggregator", password), required=False)
    for who, writer in writers.items():
        if not writer.is_connected():
            continue
        acks.clear()
        writer.on_publish = lambda c, u, mid, reason, props: acks.__setitem__(mid, reason.value)
        info = writer.publish(probe, b"smoke", qos=1, retain=False)
        if not wait_for(lambda mid=info.mid: mid in acks):
            failures.append(f"{who}: publish was not answered")
        elif acks[info.mid] < 0x80:
            failures.append(f"{who}: publish was accepted (reason code {acks[info.mid]})")
    if wait_for(lambda: probe in first, timeout=1):
        failures.append("published message was delivered")

    for client in (reader, *writers.values()):
        client.disconnect()
        client.loop_stop()

    if failures:
        sys.exit("\n".join(f"FAIL: {f}" for f in failures))
    print(f"OK: {len(TOPICS)} retained topics, publishing refused")


if __name__ == "__main__":
    main()
