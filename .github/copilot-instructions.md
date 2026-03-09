# Copilot Instructions — Din Clean App (Pebble)

## Build & Deploy Workflow

**Whenever a change touches any source file (`src/`, `package.json`, `resources/`) OR the configuration page (`config/index.html`), you MUST complete ALL of the following steps — no exceptions:**

### 1. Build
```bash
cd "/home/christophe/pebble-projects/Din Clean/App"
pebble build
```
Verify that the build succeeds and that "Free RAM available (heap)" for aplite is above 3 KB.

### 2. Commit & push to main
```bash
git add -A
git commit -m "<descriptive message>"
git push origin main
```

### 3. Install on watch and check logs
```bash
pebble install --phone 192.168.1.170
timeout 30 pebble logs --phone 192.168.1.170
```
- Inspect logs for crashes, `APP_LOG` errors, or bitmap allocation failures.
- If the watch is unreachable (connection refused), try the emulator instead:
  ```bash
  pebble install --emulator aplite
  timeout 20 pebble logs --emulator aplite
  ```
- Confirm the app launched correctly (no crash, JS ready, weather/stock fetched).

### 4. Publish config page to GitHub Pages
After **every** change — whether or not `config/index.html` was modified — keep gh-pages in sync:
```bash
git checkout gh-pages
git checkout main -- config/
git commit -m "gh-pages: <short description>"
git push origin gh-pages
git checkout main
```
(If `config/` is unchanged compared to gh-pages HEAD, the commit will be empty — that's fine, skip it.)

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

## Hardware & Runtime Discoveries (hard-won)

### Aplite 24KB RAM — every byte matters
- `text + data + bss` **must stay well under 24576 bytes**. Target max 21 000 bytes to leave enough heap+stack.
- Each message key defined in `package.json` adds ~8–12 bytes to the key mapping table at runtime. 83 keys ≈ 700–1000 bytes just for the table. Remove unused keys aggressively.
- Run `arm-none-eabi-size build/aplite/pebble-app.elf` after every build. The build output also shows "Free RAM available (heap)" — keep this above 3 KB.

### Pebble task stack is only ~2 KB
- **Never declare large local variables inside draw callbacks** (`update_proc`, layer `update_proc`, etc.). GRect arrays, structs > 32 bytes, etc. all go on the stack and cause silent stack overflow → black screen / crash.
- Solution: declare them `static` inside the function, or lift them to file-scope globals.

### `update_proc` fires before `init()` completes
- Pebble calls the layer's draw function as soon as `window_stack_push()` is called, which happens *inside* `init()`. The rest of `init()` hasn't run yet at that point.
- Guard all draw functions with a global `s_init_done` flag (set at the very end of `init()`). Skip drawing if false; trigger `layer_mark_dirty()` at end of `init()` to redraw once ready.

### AppMessage: open it first, before any malloc
- If the app crashes while AppMessage is open, the 1024-byte pool fragment can be orphaned and is unavailable on the next launch, leaving as little as ~200 bytes.
- Call `app_message_open()` as the **very first** operation in `init()`, before any `malloc`, layer creation, or other heap use, to reclaim the orphaned pool.

### AppMessage: inbox 512 bytes per message (real device limit)
- `app_message_open(512, 64)` declares the inbox size. Each individual `sendAppMessage` call from JS must fit in 512 bytes including dictionary overhead.
- Split large payloads across multiple chained `sendAppMessage` calls (chain via the success callback of the previous call). Two messages (dict1 + dict2) cover current weather + 3-day forecast comfortably.

### AppMessage: zero-value integer entries are silently dropped
- The Pebble SDK silently discards dictionary entries whose integer value is `0` from incoming messages.
- Encode boolean/enum values as 1/2 (never 0/1). For example: metric=2, imperial=1; vibration on=1, off=2.

### `gbitmap_create_with_resource` returns NULL when heap is tight
- Always null-check the result before using the bitmap. Especially critical in draw callbacks called during low-heap conditions (first launch after a crash, aplite).

### MenuLayer does not wrap / does not exit on boundary
- On Pebble, a `MenuLayer` does not exit when UP is pressed at the top or DOWN at the bottom — it just does nothing.
- To dismiss a menu window with UP/DOWN at boundary: attach a custom `ClickConfigProvider` to the window (not the MenuLayer), detect UP when `menu_layer_get_selected_index()` row == 0, and pop the window manually. Same for DOWN at last item.

### Physical watch logs
- `pebble logs --phone 192.168.1.170` works on the physical watch. Always use `timeout 30` to cap collection: `timeout 30 pebble logs --phone 192.168.1.170`
- Watch IP is 192.168.1.157 (may change if DHCP reassigns; check Pebble app → Settings → Developer Mode if connection refused).
