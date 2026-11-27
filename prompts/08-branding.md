# Phase 08 – Branding-Assets

Lies zuerst `CLAUDE.md` und `LICENSE-ASSETS.md`.

## Voraussetzung
Michael legt das oeradio.at-Logo als Vektor (SVG) und die Website-Farben nach `assets/source/`.
Fehlt das, frage nach und arbeite bis dahin mit neutralen Platzhaltern (Text „oeradio.at“).

## Ziel
Alle grafischen Bausteine für die Firmware, reproduzierbar aus den Quellen gebaut.

## Aufgaben
1. `assets/palette.json`: Primär-, Akzent-, Hintergrund-, Text- und Warnfarben aus der oeradio.at-Website (dunkles Theme für das Shack, ausreichend Kontrast auf IPS, WCAG-AA für Text prüfen). Daraus `firmware/main/ui/palette.h` generieren.
2. **Fonts:** eine gut lesbare Open-Source-Schrift (z. B. Inter oder Atkinson Hyperlegible, SIL OFL; Lizenzdatei daneben legen) in den Größen 12/16/24/48 px, mit Glyph-Bereich ASCII + Latin-1 (**Umlaute, ß, °**) und einigen Symbolen. Konvertierung mit `lv_font_conv` als Skript `assets/build.sh`, Ausgabe nach `firmware/main/ui/fonts/`.
3. **Icons:** Seiten-Icons 24 px (Uhr, Sonne/Band, Baum/POTA, Antenne/DX, Mesh, Zeitung, Zahnrad) plus Statusicons (WLAN-Stufen, Wolke an/aus, Akku-Stufen). Eigene SVGs oder ein Icon-Set mit kompatibler Lizenz (in `assets/ATTRIBUTION.md` vermerken), konvertiert ins LVGL-Bildformat.
4. **Boot-Splash** 320×240 und kleines Logo für die Statusleiste. Build-Option `BRANDING=oeradio|generic` (Kconfig), damit Forks ohne oeradio.at-Logo bauen können.
5. **Weltkarte** für die Graulinie: gemeinfreie Quelle (z. B. Natural Earth), auf 320×~160 gerendert, 2–4 Farben, im LVGL-Format.
6. `assets/README.md`: wie man die Assets neu baut und welche Lizenz für was gilt.

## Akzeptanzkriterien
- `assets/build.sh` baut alles reproduzierbar (Tools im Skript bzw. in einem Container gepinnt)
- Firmware baut mit `BRANDING=oeradio` und mit `BRANDING=generic`
- `assets/ATTRIBUTION.md` vollständig
