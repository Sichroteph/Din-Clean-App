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
pebble install --phone 192.168.1.133
timeout 30 pebble logs --phone 192.168.1.133
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
- `pebble logs --phone 192.168.1.133` works on the physical watch. Always use `timeout 30` to cap collection: `timeout 30 pebble logs --phone 192.168.1.133`
- **Priority IP order to try for `pebble install --phone`**: `192.168.1.133` → `192.168.1.157` → `192.168.1.170`
- Phone IP can change if DHCP reassigns; check Pebble app → Settings → Developer Mode if connection refused.

---

## Memory Optimization Playbook

### Measuring heap at runtime (ground truth)
Build numbers are insufficient — always measure on the actual device:
```c
APP_LOG(APP_LOG_LEVEL_DEBUG, "HEAP free: %d", (int)heap_bytes_free());
```
Place these calls at: end of `init()`, beginning of the main `update_proc`, and end of `update_proc`. The post-render value is the most important — it reflects steady-state heap available for AppMessage and dynamic allocations.

### Platform RAM model
| Platform | RAM budget | Code location |
|----------|-----------|---------------|
| aplite   | 24,576 B  | text+data+bss+heap+stack all in same 24KB |
| basalt / diorite | ~64KB | code in flash; only data+bss+heap+stack in RAM |

**Consequence**: reducing `.text` (code size) helps aplite but NOT basalt/diorite. Reducing `.data` or `.bss` helps **all** platforms equally (1 byte saved = 1 byte more heap). Prioritise BSS/data reductions when targeting all platforms.

### Analyse before touching anything
Run `arm-none-eabi-size` on all three ELFs to get a baseline:
```bash
arm-none-eabi-size build/aplite/pebble-app.elf build/basalt/pebble-app.elf build/diorite/pebble-app.elf
```
Then grep BSS symbols to find which variables are biggest:
```bash
arm-none-eabi-nm --size-sort --print-size build/aplite/pebble-app.elf | grep " [bB] "
```

### Dead code / dead variable removal
1. **Grep for usage before removing**: `grep -rn "variable_name" src/c/` — if it only appears in its own declaration and one assignment, it's dead.
2. Remove entire `static` arrays that are never read after being written.
3. Remove `persist_write` / `persist_read` pairs for values that are never used downstream.
4. Remove function declarations from `.h` files and implementations from `.c` files together.
5. Dead code discovered in this project: `weather_utils_get_weekday_name`, `weather_utils_create_date_text`, `weather_utils_get_month_abbrev`, `weather_utils_build_icon_from_wmo`, four `s_weekday_lang_*[7]` arrays, four `s_month_abbrev_*[12]` arrays; globals `icon1/2/3[20]`, `location[16]`, `rain_ico_val`, `color_right`, `color_left`.

### BSS reduction: move data out of structs into local computation
**Anti-pattern** — storing layout GRects in a persistent struct:
```c
typedef struct { GRect rect_icon; GRect rect_temp; /* ... 11 more ... */ } IconBarData;
// Assigned on every tick, stored permanently in BSS
icon_data.rect_icon = GRect(IB_ICON_X, IB_ICON_Y, 35, 35);
```
**Fix** — compute GRects as local variables inside the draw function:
```c
#define IB_ICON_X 0
#define IB_ICON_Y 60
// Inside the draw function:
GRect rect_icon = GRect(IB_ICON_X, IB_ICON_Y, 35, 35);
```
This eliminates 11 × 8 = 88 bytes of BSS and removes the assignment overhead in `update_proc`. The GRect structs only live on the stack for the ~1 ms the draw function runs.

### BSS reduction: on-demand persist loading instead of permanent BSS arrays
**Anti-pattern** — loading all persisted data at init into permanent BSS arrays:
```c
static HubMenuItem s_custom_menu_up[5];    // 5 × sizeof(HubMenuItem) in BSS always
static HubMenuItem s_custom_menu_down[5];
// Loaded unconditionally in hub_config_load()
```
**Fix** — load on demand when the feature is actually used:
```c
// In hub_menu.c, allocated only when menu window is opened:
typedef struct { HubMenuItem items[HUB_MAX_MENU_ITEMS]; ... } MenuCtx;
MenuCtx *ctx = malloc(sizeof(MenuCtx));
hub_config_load_menu(is_up, ctx->items);
// ctx is freed in window_unload — heap is reclaimed immediately after menu closes
```
This converts permanent BSS into temporary heap, net gain = `2 × HUB_MAX_MENU_ITEMS × sizeof(HubMenuItem)` bytes of BSS freed permanently.

### Reduce buffer sizes aggressively
- Always use the actual maximum string length + 1, not a round number: `char days_icon[5][16]` instead of `[5][20]` if icon names are at most 15 chars.
- `wind_unit_str[5]` (not `[6]`) for strings like "km/h" (4 chars + NUL).
- Each 4-byte shave × N array elements adds up quickly in BSS.

### AppMessage inbox size
`app_message_open(512, 32)` — inbox=512 is sufficient if JS sends data in two dicts. Outbox 32 bytes (only one `KEY_REQUEST` uint8 tuple needed). Larger inbox = less heap available for the rest of init.

### Removing struct fields vs. function parameters
When removing a field from a struct passed to a draw function, prefer **passing the value as a direct parameter** rather than restructuring the whole call:
```c
// Before: uses struct field
static void draw_wind_overlays(GContext *ctx, GRect bounds, const IconBarData *d);
// After: field removed from struct, passed directly
static void draw_wind_overlays(GContext *ctx, GRect bounds, int wind_val, int met_unit);
```

### Verified build sizes (post-optimization, March 2026)
| Platform | text   | data | bss | total  |
|----------|--------|------|-----|--------|
| aplite   | 19,463 | 504  | 416 | 20,383 |
| basalt   | ~18,600| 536  | 440 | ~19,576|
| diorite  | ~18,600| 536  | 440 | ~19,576|

Runtime heap (aplite, post-render): **~2,600 bytes free** (was ~572 before optimizations).

### Checklist before any new feature
Before adding a new feature, verify that it does not:
- Add new BSS arrays larger than ~20 bytes
- Add new global struct fields that could be computed locally instead
- Increase inbox size above 512 bytes
- Allocate inside a draw callback without freeing (leak per render cycle)
- Add `persist_read` in `init()` for data that could be loaded on demand
