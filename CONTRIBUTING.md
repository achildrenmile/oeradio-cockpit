# Mitmachen

Danke für dein Interesse an oeradio-cockpit! Es gilt der [Verhaltenskodex](CODE_OF_CONDUCT.md).
Lies vor dem ersten Beitrag [`docs/architecture.md`](docs/architecture.md),
[`docs/mqtt-topics.md`](docs/mqtt-topics.md) und die [ADRs](docs/adr/).

## Keine Secrets

Dieses Repo ist **öffentlich**. Keine Tokens, Passwörter, Tunnel-Credentials, WLAN-Daten oder
privaten Hostnamen committen. Beispieldateien heißen `*.example.*`. Die CI prüft mit gitleaks,
das ersetzt aber nicht den eigenen Blick in den Diff. Ist doch etwas gelandet: das Secret sofort
ungültig machen, Löschen allein reicht nicht.

## Branching und Pull Requests

- `phase/NN-name` für die Build-Phasen aus [`prompts/`](prompts/)
- `feat/name` für alle anderen Änderungen
- Änderungen kommen per Pull Request nach `main`. Die Definition of Done steht in der PR-Vorlage.

## Commits

Kleine Commits nach [Conventional Commits](https://www.conventionalcommits.org/de/):
`feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `ci:`, `chore:`.

## Sprache

Code, Bezeichner und Code-Kommentare auf Englisch. Doku, ADRs und UI-Texte auf Deutsch.

## Schnittstelle und Entscheidungen

- Wer `schemas/` oder `docs/mqtt-topics.md` ändert, passt Aggregator und Firmware im selben PR an
  und erhöht bei nicht abwärtskompatiblen Änderungen die Schema-Version.
- Architekturentscheidungen kommen als neue Datei nach `docs/adr/`
  (Vorlage: [`0000-template.md`](docs/adr/0000-template.md)). Bestehende ADRs werden ersetzt, nicht umgeschrieben.

## Lokal bauen

### Aggregator

_Platzhalter, wird in Phase 02 ergänzt._

### Firmware

_Platzhalter, wird in Phase 05 ergänzt._
