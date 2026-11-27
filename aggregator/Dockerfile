# syntax=docker/dockerfile:1
FROM ghcr.io/astral-sh/uv:0.13.10 AS uv

FROM python:3.12-slim AS build
COPY --from=uv /uv /usr/local/bin/uv
ENV UV_PYTHON_DOWNLOADS=never UV_COMPILE_BYTECODE=1 UV_LINK_MODE=copy
WORKDIR /app
# Dependencies first, so source changes do not invalidate this layer.
COPY pyproject.toml uv.lock README.md ./
RUN uv sync --locked --no-dev --no-install-project
COPY src ./src
RUN uv sync --locked --no-dev --no-editable

FROM python:3.12-slim
RUN useradd --system --uid 10001 --no-create-home --shell /usr/sbin/nologin cockpit
COPY --from=build /app/.venv /app/.venv
ENV PATH=/app/.venv/bin:$PATH \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    COCKPIT_HEALTH_FILE=/tmp/cockpit.health
USER 10001:10001
# Works with a read-only root filesystem as long as /tmp is writable (tmpfs).
HEALTHCHECK --interval=30s --timeout=10s --start-period=20s CMD ["python", "-m", "cockpit", "healthcheck"]
ENTRYPOINT ["python", "-m", "cockpit"]
CMD ["run"]
