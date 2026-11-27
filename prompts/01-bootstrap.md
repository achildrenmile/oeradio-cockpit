# Phase 01 – Repo-Bootstrap und CI-Gerüst

Lies zuerst `CLAUDE.md`, `README.md`, `docs/architecture.md`, `docs/mqtt-topics.md` und alle Dateien in `docs/adr/`.

## Ziel
Ein sauberes, öffentliches Monorepo-Gerüst, in dem alle späteren Phasen ihren Platz haben und
die CI von Anfang an läuft.

## Aufgaben
1. Verzeichnisse mit kurzer `README.md` je Modul anlegen: `aggregator/`, `firmware/`, `schemas/v1/`,
   `deploy/`, `assets/`, `web/`. Jede README sagt in 3–5 Sätzen, was dort hinkommt und auf welche Phase es wartet.
2. `CONTRIBUTING.md` (Deutsch): Branching (`phase/*`, `feat/*`), Conventional Commits, wie man
   Aggregator und Firmware lokal baut (Platzhalter, die spätere Phasen füllen), Hinweis „keine Secrets“.
3. `CODE_OF_CONDUCT.md` (Contributor Covenant 2.1, Kontakt: Platzhalter `<kontakt@oeradio.at>` – im PR erwähnen, dass Michael die Adresse bestätigen muss).
4. `SECURITY.md`: Meldeweg für Sicherheitsprobleme (GitHub Private Vulnerability Reporting).
5. `.github/`:
   - `ISSUE_TEMPLATE/` mit Bug (Firmware-Version, Gerät, Seite) und Feature
   - `pull_request_template.md` mit der Definition of Done aus `CLAUDE.md`
   - `dependabot.yml` für `github-actions`, `pip` (Pfad `aggregator/`)
   - Workflow `ci.yml` mit Jobs, die bei leerem Modul sauber überspringen (Pfad-Filter):
     `aggregator` (uv, ruff, mypy, pytest), `firmware` (ESP-IDF-Build im Container `espressif/idf:release-v5.x`), `schemas` (Drift-Check, vorerst Platzhalter)
   - Workflow `secret-scan.yml` mit gitleaks bei jedem Push und PR
6. `.editorconfig` (UTF-8, LF, 4 Spaces Python/C, 2 Spaces YAML/JSON/MD)

## Akzeptanzkriterien
- `git ls-files` zeigt die Struktur, alle Modul-READMEs sind vorhanden
- Die CI ist im (noch leeren) Zustand grün
- gitleaks läuft und findet nichts
- Keine Lizenzdateien verändert

## Nicht-Ziele
Kein Aggregator- oder Firmware-Code in dieser Phase.
