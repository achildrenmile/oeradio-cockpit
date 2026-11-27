"""Adapter for the JSON of the MeshCore observer (meshcore-packet-capture format).

Another observer format only needs a new ``ObserverAdapter``. The channel decryption is
ported from https://github.com/achildrenmile/meshfeed (public domain, Unlicense).
"""

import hashlib
import hmac
import json
from collections.abc import Sequence
from dataclasses import dataclass
from typing import Protocol

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

PUBLIC_KEY = bytes.fromhex("8b3387e9c5cdea6ac9e5edbaa115cd72")
_PACKET_ADVERT = "4"
_PACKET_GROUP_TEXT = "5"
_PAYLOAD_GROUP_TEXT = 0x05


@dataclass(frozen=True)
class Channel:
    label: str
    key: bytes

    @classmethod
    def from_name(cls, name: str) -> "Channel":
        """``public`` has a fixed key, a hashtag channel derives it from its name."""
        if name.lower() == "public":
            return cls("Public", PUBLIC_KEY)
        if not name.startswith("#"):
            raise ValueError(f"not a public channel: {name!r}")
        return cls(name, hashlib.sha256(name.lower().encode()).digest()[:16])


@dataclass(frozen=True)
class NodeSeen:
    key: str
    name: str
    repeater: bool


@dataclass(frozen=True)
class ChannelText:
    packet: str  # identifies the same message heard over several paths
    sender: str
    channel: str
    text: str


Event = NodeSeen | ChannelText


class ObserverAdapter(Protocol):
    def parse(self, payload: bytes) -> Event | None:
        """Return ``None`` for irrelevant packets, raise ``ValueError`` for unknown formats."""
        ...


def _offsets(raw: bytes, path: Sequence[str]) -> list[int]:
    """Candidate payload positions; the MAC decides which one is right.

    Layout: header(1) [transport(4)] path_len(1) path payload. The meaning of path_len
    differs between firmware versions.
    """
    index = 1 + (4 if raw[0] & 0x03 in (0x00, 0x03) else 0)
    if index >= len(raw):
        return []
    length = raw[index]
    sizes = [sum(len(hop) // 2 for hop in path)] if path else []
    sizes += [length, *((length & 0x3F) * size for size in (2, 1, 3))]
    return list(dict.fromkeys(o for size in sizes if (o := index + 1 + size) <= len(raw) - 3))


def decrypt_group_text(
    raw: bytes, channels: Sequence[Channel], path: Sequence[str] = ()
) -> tuple[Channel, str] | None:
    """``None`` unless the packet belongs to one of the given channels."""
    if len(raw) < 4 or (raw[0] >> 2) & 0x0F != _PAYLOAD_GROUP_TEXT:
        return None
    for offset in _offsets(raw, path):
        payload = raw[offset:]
        mac, ciphertext = payload[1:3], payload[3:]
        if len(ciphertext) < 16 or len(ciphertext) % 16:
            continue
        for channel in channels:
            if hashlib.sha256(channel.key).digest()[0] != payload[0]:
                continue
            digest = hmac.new(channel.key + bytes(16), ciphertext, hashlib.sha256).digest()
            if not hmac.compare_digest(digest[:2], mac):
                continue
            decryptor = Cipher(algorithms.AES(channel.key), modes.ECB()).decryptor()
            plain = decryptor.update(ciphertext) + decryptor.finalize()
            # timestamp(4) flags(1) NUL terminated text
            return channel, plain[5:].split(b"\x00")[0].decode("utf-8", errors="ignore")
    return None


class PacketCaptureAdapter:
    def __init__(self, channels: Sequence[Channel]) -> None:
        self._channels = channels

    def parse(self, payload: bytes) -> Event | None:
        data = json.loads(payload)
        if not isinstance(data, dict) or "type" not in data:
            raise ValueError("not an observer packet")
        if data["type"] != "PACKET":
            return None
        decoded = data.get("decoded")
        if not isinstance(decoded, dict):
            decoded = {}
        match data.get("packet_type"):
            case "4":
                return self._advert(decoded)
            case "5":
                return self._text(data, decoded)
        return None

    def _advert(self, decoded: dict[str, object]) -> NodeSeen | None:
        # advert_parse_ok means the observer verified the advert.
        key = decoded.get("public_key")
        if not decoded.get("advert_parse_ok") or not isinstance(key, str):
            return None
        name = str(decoded.get("name") or "").strip()
        return NodeSeen(key.lower(), name or key[:8], decoded.get("mode") == "Repeater")

    def _text(self, data: dict[str, object], decoded: dict[str, object]) -> ChannelText | None:
        hex_data = data.get("raw")
        if not isinstance(hex_data, str):
            return None
        path = decoded.get("path")
        if not isinstance(path, list):
            path = str(data["path"]).split(",") if data.get("path") else []
        result = decrypt_group_text(
            bytes.fromhex(hex_data), self._channels, [str(hop) for hop in path]
        )
        if result is None:
            return None
        channel, text = result
        sender, separator, content = text.partition(": ")
        if not separator or len(sender) >= 50:
            sender, content = "?", text
        packet = str(data.get("hash") or "").lower() or hashlib.sha256(text.encode()).hexdigest()
        return ChannelText(packet, sender, channel.label, content)
