"""Latest articles from an RSS or Atom feed."""

import calendar

import feedparser
import httpx

from ..models import News, NewsItem, now
from .base import Source


def parse_news(content: bytes) -> News:
    feed = feedparser.parse(content)
    items = []
    for entry in feed.entries:
        published = entry.get("published_parsed") or entry.get("updated_parsed")
        title = " ".join(entry.get("title", "").split())
        link = entry.get("link", "")
        if published and title and link.startswith(("https://", "http://")):
            items.append(NewsItem(t=calendar.timegm(published), title=title, url=link))
    if not items:
        # Also covers error pages served with status 200.
        raise ValueError("feed contains no usable entries")
    items.sort(key=lambda item: item.t, reverse=True)
    return News(ts=now(), src_ts=items[0].t, items=items)


class NewsSource(Source):
    """Uses the ETag so an unchanged feed is not downloaded again."""

    name = "news"

    def __init__(self, client: httpx.AsyncClient, url: str, interval: float) -> None:
        self.interval = interval
        self._client = client
        self._url = url
        self._etag: str | None = None
        self._last: News | None = None

    async def fetch(self) -> News:
        headers = {"If-None-Match": self._etag} if self._etag and self._last else {}
        response = await self._client.get(self._url, headers=headers)
        if response.status_code == httpx.codes.NOT_MODIFIED and self._last:
            return self._last.model_copy(update={"ts": now()})
        response.raise_for_status()
        self._last = parse_news(response.content)
        self._etag = response.headers.get("etag")
        return self._last
