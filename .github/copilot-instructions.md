# Copilot Instructions — Din Clean App (Pebble)

## Build & Deploy Workflow

After modifying any source file (`src/`, `package.json`, `resources/`):

```bash
cd "/home/christophe/pebble-projects/Din Clean/App"
pebble build
```

After modifying `config/index.html`, always deploy to GitHub Pages so the live config page is updated:

```bash
cd "/home/christophe/pebble-projects/Din Clean/App"
git add -A
git commit -m "<descriptive message>"
git push origin main
git checkout gh-pages
git checkout main -- config/
git commit -m "gh-pages: <short description>"
git push origin gh-pages
git checkout main
```

## Install on Watch

```bash
pebble install --phone 192.168.1.170
```

## Project Structure

- **`src/c/`** — C source code (watchface + hub system)
- **`src/pkjs/js/pebble_js_app.js`** — PebbleKit JS (weather API, config, webhooks)
- **`config/index.html`** — Configuration page (hosted on GitHub Pages at `sichroteph.github.io/Din-Clean-App/config/index.html`)
- **`package.json`** — Pebble app manifest (message keys, resources, app metadata)
- **`resources/`** — Fonts and images

## Key Conventions

- **Platforms**: aplite (B&W, 24KB RAM), basalt (color, 64KB), diorite (B&W, 64KB). All forced B&W currently.
- **AppMessage budget**: 1024 bytes inbox. Use byte arrays (`TUPLE_BYTE_ARRAY`) for bulk data to save space.
- **Message keys**: Defined in `package.json` → auto-generated in `build/include/message_keys.auto.h`. Must also add `#define` in `Din_Clean.c` for runtime use.
- **Weather API**: Open-Meteo AROME (default) or MET Norway (fallback). Configured via `localStorage(180)`.
- **Widget system**: `WidgetDef` in `hub_widgets.c` with `.draw()`, `.page_count()`, `.name`. Register in `s_widget_defs[]` and enum in `hub_config.h`.
- **Config page**: Uses no external framework, just vanilla HTML/JS/CSS. `AVAILABLE_WIDGETS` and `AVAILABLE_PSEUDOAPPS` arrays must match C-side enums.
