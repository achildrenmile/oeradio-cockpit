"""Shared HTTP client."""

import httpx

from .. import __version__

USER_AGENT = f"oeradio-cockpit/{__version__} (+https://oeradio.at)"


def make_client() -> httpx.AsyncClient:
    return httpx.AsyncClient(headers={"User-Agent": USER_AGENT}, timeout=20, follow_redirects=True)


async def get(client: httpx.AsyncClient, url: str) -> httpx.Response:
    response = await client.get(url)
    response.raise_for_status()
    return response
