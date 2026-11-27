"""One MQTT connection shared by all sources."""

import asyncio
from collections.abc import Callable
from contextlib import AbstractAsyncContextManager
from typing import Any, Protocol

import aiomqtt
import structlog

from .backoff import backoff_delay
from .config import Settings
from .models import Message, Status, now
from .models.base import MAX_BYTES, WARN_BYTES

QOS = 1
KEEPALIVE = 60
RECONNECT_BASE = 1.0
RECONNECT_CAP = 60.0

log = structlog.get_logger(__name__)


class PayloadTooLargeError(ValueError):
    pass


class MqttClient(Protocol):
    @property
    def messages(self) -> Any: ...

    async def publish(self, topic: str, /, payload: bytes, qos: int, retain: bool) -> None: ...


ClientFactory = Callable[[aiomqtt.Will], AbstractAsyncContextManager[MqttClient]]


class Publisher:
    """Publishes retained messages and republishes the latest ones after a reconnect."""

    def __init__(self, settings: Settings, client_factory: ClientFactory | None = None) -> None:
        self._settings = settings
        self._factory = client_factory or self._default_client
        self._client: MqttClient | None = None
        self._latest: dict[str, bytes] = {}
        # Serialises sends so a replay can never overwrite a newer payload.
        self._lock = asyncio.Lock()

    @property
    def connected(self) -> bool:
        return self._client is not None

    def topic(self, name: str) -> str:
        return f"{self._settings.topic_prefix}/{name}"

    async def publish(self, name: str, message: Message) -> None:
        """Send now if connected, otherwise on the next connect."""
        payload = message.encoded()
        size = len(payload)
        if size > MAX_BYTES:
            log.error("payload_too_large", topic=name, bytes=size, limit=MAX_BYTES)
            raise PayloadTooLargeError(f"{name}: {size} bytes exceeds {MAX_BYTES}")
        if size > WARN_BYTES:
            log.warning("payload_large", topic=name, bytes=size, target=WARN_BYTES)
        async with self._lock:
            topic = self.topic(name)
            self._latest[topic] = payload
            if self._client is not None:
                try:
                    await self._send(self._client, topic, payload)
                except aiomqtt.MqttError as error:
                    # The payload stays cached and is sent after the reconnect.
                    log.warning("publish_deferred", topic=name, error=str(error))

    async def run(self) -> None:
        """Keep the connection up until cancelled, then announce ``offline``."""
        failures = 0
        while True:
            will = aiomqtt.Will(self.topic("status"), self._offline_payload(), qos=QOS, retain=True)
            try:
                async with self._factory(will) as client:
                    failures = 0
                    log.info("mqtt_connected", host=self._settings.broker_host)
                    await self._serve(client)
            except aiomqtt.MqttError as error:
                failures += 1
                delay = backoff_delay(failures, RECONNECT_BASE, RECONNECT_CAP)
                log.warning("mqtt_disconnected", error=str(error), retry_in=delay)
                await asyncio.sleep(delay)

    async def _serve(self, client: MqttClient) -> None:
        try:
            async with self._lock:
                for topic, payload in self._latest.items():
                    await self._send(client, topic, payload)
                self._client = client
            # Nothing is subscribed; iterating only serves to raise on disconnect.
            async for _ in client.messages:
                pass
            raise aiomqtt.MqttError("message stream ended")
        except asyncio.CancelledError:
            # A clean disconnect does not trigger the will, so publish it ourselves.
            try:
                await self._send(client, self.topic("status"), self._offline_payload())
            except aiomqtt.MqttError as error:
                log.warning("offline_publish_failed", error=str(error))
            raise
        finally:
            self._client = None

    async def _send(self, client: MqttClient, topic: str, payload: bytes) -> None:
        await client.publish(topic, payload=payload, qos=QOS, retain=True)

    def _offline_payload(self) -> bytes:
        return Status(ts=now(), offline=True).encoded()

    def _default_client(self, will: aiomqtt.Will) -> AbstractAsyncContextManager[MqttClient]:
        settings = self._settings
        password = settings.broker_password
        return aiomqtt.Client(
            hostname=settings.broker_host,
            port=settings.broker_port,
            username=settings.broker_user,
            password=password.get_secret_value() if password else None,
            tls_params=aiomqtt.TLSParameters() if settings.broker_tls else None,
            keepalive=KEEPALIVE,
            will=will,
        )
