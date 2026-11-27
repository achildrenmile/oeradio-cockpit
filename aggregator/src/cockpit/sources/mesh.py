"""Aggregate what the MeshCore observer hears. State lives in memory only."""

import asyncio
from collections.abc import Callable
from contextlib import AbstractAsyncContextManager
from typing import Any

import aiomqtt
import structlog

from ..backoff import backoff_delay
from ..models import Mesh, MeshMessage, Repeater, now
from ..models.mesh import MAX_RECENT
from .base import Source
from .meshcore import ChannelText, Event, NodeSeen, ObserverAdapter

SILENCE_LIMIT = 15 * 60
RECONNECT_BASE = 2.0
RECONNECT_CAP = 300.0
SEEN_PACKETS = 500

log = structlog.get_logger(__name__)


class MeshState:
    def __init__(self, window: float) -> None:
        self._window = window
        self._nodes: dict[str, tuple[NodeSeen, int]] = {}
        self._recent: list[MeshMessage] = []
        self._packets: dict[str, None] = {}

    def add(self, event: Event, seen: int) -> None:
        if isinstance(event, NodeSeen):
            self._nodes[event.key] = (event, seen)
        elif isinstance(event, ChannelText) and event.packet not in self._packets:
            self._packets[event.packet] = None
            if len(self._packets) > SEEN_PACKETS:
                del self._packets[next(iter(self._packets))]
            # The receive time: clocks of mesh nodes are often wrong.
            message = MeshMessage(t=seen, sender=event.sender, ch=event.channel, txt=event.text)
            self._recent = [message, *self._recent][:MAX_RECENT]

    def snapshot(self, reference: int) -> Mesh:
        limit = reference - self._window
        self._nodes = {k: v for k, v in self._nodes.items() if v[1] >= limit}
        self._recent = [m for m in self._recent if m.t >= limit]
        repeaters = sorted(
            ((node, seen) for node, seen in self._nodes.values() if node.repeater),
            key=lambda item: item[1],
            reverse=True,
        )
        return Mesh(
            ts=reference,
            nodes_online=len(self._nodes),
            repeaters_online=len(repeaters),
            recent=self._recent,
            top_repeaters=[Repeater(name=node.name, seen=seen) for node, seen in repeaters],
        )


class MeshSource(Source):
    """Subscribes to the observer broker with its own client and never publishes there."""

    name = "mesh"
    backoff = False

    def __init__(
        self,
        client_factory: Callable[[], AbstractAsyncContextManager[Any]],
        topic: str,
        adapter: ObserverAdapter,
        interval: float,
        window: float,
    ) -> None:
        self.interval = interval
        self.state = MeshState(window)
        self.connected = False
        self.last_packet: int | None = None
        self._factory = client_factory
        self._topic = topic
        self._adapter = adapter
        self._format_warned = False

    async def fetch(self) -> Mesh:
        if not self.connected:
            raise ConnectionError("not connected to the observer broker")
        if self.last_packet is None:
            raise LookupError("no observer packets received yet")
        if now() - self.last_packet > SILENCE_LIMIT:
            raise TimeoutError("the observer has gone quiet")
        return self.state.snapshot(now())

    def handle(self, payload: bytes) -> None:
        try:
            event = self._adapter.parse(payload)
        except (ValueError, KeyError, TypeError) as error:
            if not self._format_warned:
                self._format_warned = True
                log.error(
                    "mesh_unknown_format",
                    error=repr(error),
                    hint="expected meshcore-packet-capture JSON on COCKPIT_MESH_TOPIC",
                )
            return
        self.last_packet = now()
        if event:
            self.state.add(event, self.last_packet)

    async def listen(self) -> None:
        failures = 0
        while True:
            try:
                async with self._factory() as client:
                    await client.subscribe(self._topic)
                    self.connected = True
                    failures = 0
                    log.info("mesh_connected")
                    async for message in client.messages:
                        # Retained packets are old news, not current activity.
                        if not message.retain:
                            self.handle(bytes(message.payload))
            except aiomqtt.MqttError as error:
                log.warning("mesh_disconnected", error=str(error))
            self.connected = False
            failures += 1
            await asyncio.sleep(backoff_delay(failures, RECONNECT_BASE, RECONNECT_CAP))
