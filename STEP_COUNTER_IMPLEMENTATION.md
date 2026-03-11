# Step Counter Implementation Guide

> Complete, exhaustive specification for implementing a software pedometer in the
> Din Clean App using a Pebble background worker + accelerometer.
> Written March 2026. All file contents verified against current code.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Persist Key Allocation](#2-persist-key-allocation)
3. [File Changes — Exact Diffs](#3-file-changes--exact-diffs)
   - 3a. hub_config.h
   - 3b. hub_widgets.c
   - 3c. Din_Clean.c
   - 3d. config/index.html
   - 3e. NEW: worker_src/c/step_worker.c
4. [Background Worker Algorithm](#4-background-worker-algorithm)
5. [Widget Rendering Spec](#5-widget-rendering-spec)
6. [Build System](#6-build-system)
7. [Memory & Binary Budget](#7-memory--binary-budget)
8. [Testing Checklist](#8-testing-checklist)
9. [Reference: Current Code State](#9-reference-current-code-state)

---

## 1. Architecture Overview

```
worker_src/c/step_worker.c          App/src/c/ (≤400B added code)
┌─────────────────────────┐         ┌─────────────────────────────┐
│ 10.5KB separate RAM     │         │ 24KB heap (unchanged)       │
│                         │         │                             │
│ accel_service_peek()    │         │ widget_steps_draw():        │
│ every 500ms via timer   │         │   persist_read_int(230)     │
│ ↓                       │         │   persist_read_int(231-237) │
│ delta-threshold algo    │         │   → total + 7 bar histogram │
│ ↓                       │         │                             │
│ persist_write_int(230)  │──persist─→│ app_worker_launch() in    │
│ every 60s               │  shared │   init() (fire-and-forget)  │
│                         │         │                             │
│ midnight rotation:      │         │ No globals, no heap alloc,  │
│ shift day history       │         │ no AppWorkerMessage handler  │
└─────────────────────────┘         └─────────────────────────────┘
```

**Communication**: persist_storage (shared between worker and app).
No AppWorkerMessage needed — same pattern as the existing Stock widget
(JS writes persist → widget reads persist on draw).

**Why a background worker?** Without it, steps are only counted while the app is
in the foreground. The worker runs continuously, even when other watchfaces/apps
are active. It has its own 10.5KB RAM, completely separate from the app's 24KB.

---

## 2. Persist Key Allocation

The range 228–299 is currently free. Keys used:

| Key | Define                    | Type      | Content                          |
|-----|---------------------------|-----------|----------------------------------|
| 230 | `HUB_PERSIST_STEPS_TODAY` | uint32_t  | Steps counted today              |
| 231 | `HUB_PERSIST_STEPS_DAY0`  | uint16_t  | Steps yesterday (day -1)        |
| 232 | `HUB_PERSIST_STEPS_DAY1`  | uint16_t  | Steps day -2                    |
| 233 | `HUB_PERSIST_STEPS_DAY2`  | uint16_t  | Steps day -3                    |
| 234 | `HUB_PERSIST_STEPS_DAY3`  | uint16_t  | Steps day -4                    |
| 235 | `HUB_PERSIST_STEPS_DAY4`  | uint16_t  | Steps day -5                    |
| 236 | `HUB_PERSIST_STEPS_DAY5`  | uint16_t  | Steps day -6                    |
| 237 | `HUB_PERSIST_STEPS_DAY6`  | uint16_t  | Steps day -7                    |
| 238 | `HUB_PERSIST_STEPS_DATE`  | int32_t   | Julian day number of "today"    |

**Existing persist key map** (for reference, do not change):
- 0–55, 103, 113–115, 120, 126–133, 149, 200–227: Weather, pool, config
- 300–304: Hub config
- 310–314: Stock panels
- 320: Countdown
- 350–365: Hub message keys
- 370–372: Stock data + diagnostics

---

## 3. File Changes — Exact Diffs

### 3a. `App/src/c/hub_config.h`

**Current enum** (lines 83–88):
```c
typedef enum {
  HUB_WIDGET_STOCKS = 0,
  HUB_WIDGET_WEATHER_HOURLY,
  HUB_WIDGET_WEATHER_DAILY,
  HUB_WIDGET_COUNT
} HubWidgetId;
```

**Change to:**
```c
typedef enum {
  HUB_WIDGET_STOCKS = 0,
  HUB_WIDGET_WEATHER_HOURLY,
  HUB_WIDGET_WEATHER_DAILY,
  HUB_WIDGET_STEPS,
  HUB_WIDGET_COUNT
} HubWidgetId;
```

**Add persist key defines** — after the existing `HUB_PERSIST_STOCK4 314` line
(after line 47):

```c
// Step counter persist keys (written by background worker, read by widget)
#define HUB_PERSIST_STEPS_TODAY 230
#define HUB_PERSIST_STEPS_DAY0  231  // yesterday
#define HUB_PERSIST_STEPS_DATE  238  // julian day of "today"
```

No other changes to hub_config.h.

---

### 3b. `App/src/c/hub_widgets.c`

**3b-i. Add forward declarations** — after line 33 (`static void widget_daily_draw...`):

```c
static void widget_steps_draw(GContext *ctx, GRect bounds, uint8_t page);
static uint8_t widget_steps_page_count(void);
```

**3b-ii. Add entry to s_widget_defs[]** — current array (lines 44–49):

```c
static const WidgetDef s_widget_defs[] = {
    [HUB_WIDGET_STOCKS] = {widget_stocks_draw, widget_stocks_page_count},
    [HUB_WIDGET_WEATHER_HOURLY] = {widget_hourly_draw,
                                   widget_hourly_page_count},
    [HUB_WIDGET_WEATHER_DAILY] = {widget_daily_draw, widget_daily_page_count},
};
```

**Change to:**
```c
static const WidgetDef s_widget_defs[] = {
    [HUB_WIDGET_STOCKS] = {widget_stocks_draw, widget_stocks_page_count},
    [HUB_WIDGET_WEATHER_HOURLY] = {widget_hourly_draw,
                                   widget_hourly_page_count},
    [HUB_WIDGET_WEATHER_DAILY] = {widget_daily_draw, widget_daily_page_count},
    [HUB_WIDGET_STEPS] = {widget_steps_draw, widget_steps_page_count},
};
```

**3b-iii. Add widget implementation** — at the very end of the file (after the
`widget_daily_draw` function, which ends around line 597):

```c
// ========== Steps Widget ==========

static uint8_t widget_steps_page_count(void) { return 1; }

static void widget_steps_draw(GContext *ctx, GRect bounds, uint8_t page) {
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);

  // Read today's step count from persist (written by background worker)
  uint32_t today = 0;
  if (persist_exists(HUB_PERSIST_STEPS_TODAY)) {
    today = (uint32_t)persist_read_int(HUB_PERSIST_STEPS_TODAY);
  }

  // Display today's count (large, centered)
  char buf[8];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)today);
  GFont font28b = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  graphics_draw_text(ctx, buf, font28b, GRect(0, 10, bounds.size.w, 34),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);

  // Label
  GFont font14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_draw_text(ctx, "steps today", font14,
                     GRect(0, 44, bounds.size.w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);

  // --- 7-day history histogram ---
  // Read last 7 days from persist
  uint16_t hist[7];
  uint16_t hist_max = 1; // avoid division by zero
  for (int i = 0; i < 7; i++) {
    hist[i] = 0;
    int key = HUB_PERSIST_STEPS_DAY0 + i;
    if (persist_exists(key)) {
      int32_t v = persist_read_int(key);
      hist[i] = (v > 0 && v < 65535) ? (uint16_t)v : 0;
    }
    if (hist[i] > hist_max)
      hist_max = hist[i];
  }

  // Bar chart: 7 bars, bottom-aligned
  int bar_area_top = 72;
  int bar_area_bot = 148;
  int bar_h_max = bar_area_bot - bar_area_top;
  int bar_w = 14;
  int gap = (bounds.size.w - 7 * bar_w) / 8; // even spacing

  for (int i = 0; i < 7; i++) {
    int bh = (int)hist[i] * bar_h_max / hist_max;
    if (hist[i] > 0 && bh < 2)
      bh = 2; // minimum visible bar
    int bx = gap + i * (bar_w + gap);
    int by = bar_area_bot - bh;
    if (bh > 0) {
      graphics_fill_rect(ctx, GRect(bx, by, bar_w, bh), 0, GCornerNone);
    }
  }

  // Day labels below bars: -7 -6 -5 -4 -3 -2 -1
  for (int i = 0; i < 7; i++) {
    char lbl[4];
    snprintf(lbl, sizeof(lbl), "-%d", 7 - i);
    int bx = gap + i * (bar_w + gap);
    graphics_draw_text(ctx, lbl, font14,
                       GRect(bx - 2, bar_area_bot, bar_w + 4, 14),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  }
}
```

**Cost analysis for added code in hub_widgets.c:**
- `widget_steps_page_count()`: ~8B .text
- `widget_steps_draw()`: ~220B .text (persist reads, snprintf, 2× draw_text, bar loop)
- `s_widget_defs[3]` entry: 8B .rodata (2 function pointers)
- String literals ("steps today", format strings): ~20B .rodata
- **Total: ~256B**

---

### 3c. `App/src/c/Din_Clean.c`

**Current init()** function (lines 1169–1206). The relevant section to modify
is lines 1199–1206:

```c
  hub_set_main_window(s_main_window);
  hub_timeout_init(hub_timeout_fired);

  // Mark init complete — callbacks (focus, tick) may now safely send
  // AppMessages.
  s_init_done = true;
```

**Change to:**
```c
  hub_set_main_window(s_main_window);
  hub_timeout_init(hub_timeout_fired);

  // Launch step counter background worker (fire-and-forget, errors ignored)
  app_worker_launch();

  // Mark init complete — callbacks (focus, tick) may now safely send
  // AppMessages.
  s_init_done = true;
```

**Cost**: ~16B .text (one function call). No new includes needed (`app_worker_launch`
is in `<pebble.h>` which is already included).

**No change to deinit()**: The worker persists independently. Killing it in
deinit would stop step counting when the app closes, which is the opposite of
what we want.

---

### 3d. `App/config/index.html`

**Current AVAILABLE_WIDGETS array** (around line 1330):
```js
    var AVAILABLE_WIDGETS = [
      { id: 0, name: 'Stocks', desc: 'Configurable panels with price graph' },
      { id: 1, name: 'Hourly Weather', desc: '4 pages of 12h (48h forecast)' },
      { id: 2, name: 'Daily Weather', desc: '7-day forecast, 2 days per page' }
    ];
```

**Change to:**
```js
    var AVAILABLE_WIDGETS = [
      { id: 0, name: 'Stocks', desc: 'Configurable panels with price graph' },
      { id: 1, name: 'Hourly Weather', desc: '4 pages of 12h (48h forecast)' },
      { id: 2, name: 'Daily Weather', desc: '7-day forecast, 2 days per page' },
      { id: 3, name: 'Steps', desc: 'Step counter + 7-day history' }
    ];
```

**No other HTML/JS changes needed.** Widget IDs are simple integers passed in
the config string. The JS parsing code (`hub_config_parse_widgets`) handles
arbitrary IDs — it just stores the numeric array.

**After modifying, deploy to gh-pages:**
```bash
git checkout gh-pages && git checkout main -- config/ && \
git commit -m "gh-pages: add Steps widget option" && \
git push origin gh-pages && git checkout main
```

---

### 3e. NEW FILE: `App/worker_src/c/step_worker.c`

Create the directory `App/worker_src/c/` and this file. The wscript already
detects `worker_src/` automatically (line 44: `build_worker = os.path.exists('worker_src')`)
and builds `pebble-worker.elf` from `worker_src/c/**/*.c`.

```c
#include <pebble_worker.h>

// --- Persist keys (shared with app) ---
#define PERSIST_STEPS_TODAY 230
#define PERSIST_STEPS_DAY0  231  // yesterday (DAY0..DAY6 = 231..237)
#define PERSIST_STEPS_DATE  238  // julian day of current "today"

// --- Algorithm parameters ---
#define POLL_INTERVAL_MS  500   // accelerometer poll rate (2 Hz)
#define PERSIST_INTERVAL  120   // persist every N polls (120 × 500ms = 60s)
#define MAG_THRESHOLD     340   // magnitude delta threshold for step detection
#define MIN_STEP_INTERVAL 1     // minimum polls between steps (500ms debounce)

// --- State ---
static uint32_t s_steps_today;
static int16_t  s_last_mag;
static uint8_t  s_polls_since_persist;
static uint8_t  s_polls_since_step;
static int32_t  s_today_jday;       // julian day number of "today"
static AppTimer *s_timer;

// --- Helpers ---

// Simple julian day number from time_t (days since epoch, ignoring TZ drift)
static int32_t jday_from_time(time_t t) {
  struct tm *tm = localtime(&t);
  // Approximate: just use yday + year*366 for a monotonic day counter
  return tm->tm_year * 366 + tm->tm_yday;
}

// Rotate history: shift DAY0-5 → DAY1-6, DAY0 = today's count, reset today
static void rotate_day(void) {
  // Shift existing days forward
  for (int i = 5; i >= 0; i--) {
    int32_t val = 0;
    if (persist_exists(PERSIST_STEPS_DAY0 + i)) {
      val = persist_read_int(PERSIST_STEPS_DAY0 + i);
    }
    persist_write_int(PERSIST_STEPS_DAY0 + i + 1, val);
  }
  // Yesterday = today's count (cap at uint16 max for persist)
  uint16_t capped = (s_steps_today > 65535) ? 65535 : (uint16_t)s_steps_today;
  persist_write_int(PERSIST_STEPS_DAY0, capped);
  // Reset today
  s_steps_today = 0;
  persist_write_int(PERSIST_STEPS_TODAY, 0);
  // Update date
  s_today_jday = jday_from_time(time(NULL));
  persist_write_int(PERSIST_STEPS_DATE, s_today_jday);
}

// --- Timer callback: poll accelerometer ---
static void timer_callback(void *data) {
  AccelData accel = (AccelData){ .x = 0, .y = 0, .z = 0 };
  accel_service_peek(&accel);

  // Skip samples contaminated by vibration motor
  if (!accel.did_vibrate) {
    // Approximate magnitude using Manhattan distance (no sqrt needed)
    // Gravity is ~1000 on z-axis at rest; we care about changes
    int16_t mag = (int16_t)((abs(accel.x) + abs(accel.y) + abs(accel.z)));
    int16_t delta = abs(mag - s_last_mag);

    // Step detection: delta exceeds threshold + debounce
    if (delta > MAG_THRESHOLD && s_polls_since_step >= MIN_STEP_INTERVAL) {
      s_steps_today++;
      s_polls_since_step = 0;
    } else {
      if (s_polls_since_step < 255)
        s_polls_since_step++;
    }

    s_last_mag = mag;
  }

  // Persist periodically
  s_polls_since_persist++;
  if (s_polls_since_persist >= PERSIST_INTERVAL) {
    persist_write_int(PERSIST_STEPS_TODAY, (int32_t)s_steps_today);
    s_polls_since_persist = 0;
  }

  // Check for day change
  int32_t now_jday = jday_from_time(time(NULL));
  if (now_jday != s_today_jday) {
    rotate_day();
  }

  // Reschedule
  s_timer = app_timer_register(POLL_INTERVAL_MS, timer_callback, NULL);
}

// --- Init / Deinit ---

static void prv_init(void) {
  // Load saved state
  s_today_jday = persist_exists(PERSIST_STEPS_DATE)
                     ? persist_read_int(PERSIST_STEPS_DATE)
                     : 0;

  int32_t now_jday = jday_from_time(time(NULL));

  if (s_today_jday != now_jday && s_today_jday != 0) {
    // Day has changed since last run — rotate history
    // Handle multi-day gap: rotate once per missed day
    int32_t gap = now_jday - s_today_jday;
    if (gap < 0 || gap > 7) gap = 1; // safety clamp
    for (int32_t d = 0; d < gap; d++) {
      rotate_day();
    }
    s_today_jday = now_jday;
    persist_write_int(PERSIST_STEPS_DATE, s_today_jday);
  } else if (s_today_jday == 0) {
    // First ever run
    s_today_jday = now_jday;
    persist_write_int(PERSIST_STEPS_DATE, s_today_jday);
    s_steps_today = 0;
    persist_write_int(PERSIST_STEPS_TODAY, 0);
  } else {
    // Same day — resume count
    s_steps_today = persist_exists(PERSIST_STEPS_TODAY)
                        ? (uint32_t)persist_read_int(PERSIST_STEPS_TODAY)
                        : 0;
  }

  s_last_mag = 0;
  s_polls_since_persist = 0;
  s_polls_since_step = MIN_STEP_INTERVAL; // allow immediate first step

  // Subscribe to accelerometer (needed for accel_service_peek)
  accel_data_service_subscribe(0, NULL);

  // Start polling timer
  s_timer = app_timer_register(POLL_INTERVAL_MS, timer_callback, NULL);
}

static void prv_deinit(void) {
  // Persist final count before exit
  persist_write_int(PERSIST_STEPS_TODAY, (int32_t)s_steps_today);

  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  accel_data_service_unsubscribe();
}

int main(void) {
  prv_init();
  worker_event_loop();
  prv_deinit();
}
```

**Worker memory footprint** (~20B BSS):
- `s_steps_today`: 4B
- `s_last_mag`: 2B
- `s_polls_since_persist`: 1B
- `s_polls_since_step`: 1B
- `s_today_jday`: 4B
- `s_timer`: 4B
- Total: 16B BSS + stack locals during callbacks

**Worker .text estimate**: ~600-800B (well within 10.5KB limit)

---

## 4. Background Worker Algorithm

### Step Detection: Delta-Threshold

Based on the algorithm from [jathusanT/pebble_pedometer](https://github.com/jathusanT/pebble_pedometer)
(42 stars, GPL-3), simplified for lower CPU overhead:

1. **Poll** `accel_service_peek()` every 500ms (2 Hz)
2. **Compute magnitude**: `|x| + |y| + |z|` (Manhattan distance, no sqrt)
   - At rest: ~1000 (gravity on z-axis)
   - Walking: 1200-1800 peaks, 600-900 valleys
3. **Detect step**: `|current_mag - last_mag| > MAG_THRESHOLD`
4. **Debounce**: minimum 1 poll (500ms) between steps
5. **Filter vibration**: skip sample if `accel.did_vibrate == true`

### Calibration

The `MAG_THRESHOLD` value (default 340) can be tuned:
- **Lower** (200-300): more sensitive, more false positives from arm gestures
- **Higher** (400-500): less sensitive, misses light steps
- **340**: good starting point for normal walking

A future enhancement could expose sensitivity in the config page and pass it
to the worker via persist.

### Day Rotation

Triggered when `jday_from_time(time(NULL)) != s_today_jday`:
1. Shift `DAY0..DAY5` → `DAY1..DAY6`
2. `DAY0` = today's count (capped to uint16)
3. Reset `s_steps_today = 0`
4. Update `s_today_jday`

Multiple missed days (e.g., watch was off for 3 days) are handled by
rotating once per gap day.

### Persist Strategy

- Write `PERSIST_STEPS_TODAY` every 60 seconds (120 polls × 500ms)
- This balances flash wear vs data loss risk
- Worker `prv_deinit()` always persists the final value
- Flash endurance: Pebble persist uses external SPI flash rated for 100K+ cycles.
  At 1 write/min, that's 69 days continuous — but Pebble's wear leveling extends
  this massively. No concern.

---

## 5. Widget Rendering Spec

### Layout (144×168 screen)

```
┌──────────────────────────────┐
│          (y=10)              │
│        12,345                │  ← GOTHIC_28_BOLD, centered
│      steps today             │  ← GOTHIC_14, centered (y=44)
│                              │
│  ┌──┐    ┌──┐ ┌──┐          │
│  │  │┌──┐│  │ │  │┌──┐┌──┐  │  ← 7 bars, top=72, bottom=148
│  │  ││  ││  │ │  ││  ││  │  │     14px wide, proportional to max
│  │  ││  ││  │ │  ││  ││  │  │
│  │  ││  ││  │ │  ││  ││  │┌─┐│
│  └──┘└──┘└──┘ └──┘└──┘└──┘└─┘│
│  -7  -6  -5  -4  -3  -2  -1 │  ← GOTHIC_14 labels (y=148)
│                         1/3  │  ← position indicator (drawn by framework)
└──────────────────────────────┘
```

### Data Sources (all from persist, zero heap)

| Data | Persist Key | Read as |
|------|------------|---------|
| Today's steps | 230 | `persist_read_int()` → uint32_t |
| Days -1 to -7 | 231-237 | `persist_read_int()` → uint16_t |

### Fonts Used (all system fonts, zero resource loading cost)

| Font | Usage |
|------|-------|
| `FONT_KEY_GOTHIC_28_BOLD` | Today's count |
| `FONT_KEY_GOTHIC_14` | "steps today" label + day labels |

---

## 6. Build System

### wscript (NO CHANGES NEEDED)

The existing wscript (verified lines 44-59) already handles workers:

```python
build_worker = os.path.exists('worker_src')
# ...
if build_worker:
    worker_elf='{}/pebble-worker.elf'.format(p)
    binaries.append({'platform': p, 'app_elf': app_elf, 'worker_elf': worker_elf})
    ctx.pbl_worker(source=ctx.path.ant_glob('worker_src/c/**/*.c'),
    target=worker_elf)
```

**Simply creating `App/worker_src/c/step_worker.c` is enough** — the wscript
will detect `worker_src/` and build the worker automatically.

### package.json (NO CHANGES NEEDED)

The worker is built from the wscript, not declared in package.json. The existing
capabilities `["location", "configurable"]` are sufficient.

### Build & Deploy Commands

```bash
cd "/home/christophe/pebble-projects/Din Clean/App"

# Build (worker is auto-detected and compiled)
pebble build

# Check sizes
arm-none-eabi-size build/aplite/pebble-app.elf
arm-none-eabi-size build/aplite/pebble-worker.elf

# Install
pebble install --phone 192.168.1.170

# Deploy config page to gh-pages (only if index.html changed)
git add -A && git commit -m "feat: add step counter worker + widget"
git push origin main
git checkout gh-pages && git checkout main -- config/ && \
git commit -m "gh-pages: add Steps widget" && \
git push origin gh-pages && git checkout main
```

---

## 7. Memory & Binary Budget

### Baseline (current, verified March 11, 2026)

```
$ arm-none-eabi-size build/aplite/pebble-app.elf
   text    data     bss     dec     hex filename
  19892     220     287   20399    4faf build/aplite/pebble-app.elf
```

Aplite limit: 24,576B total (text + data + bss + heap + stack)
Available: 24,576 - 20,399 = **4,177B** remaining

### Expected delta (app only)

| Component | .text | .rodata | .bss | heap |
|-----------|-------|---------|------|------|
| `widget_steps_draw()` | ~220B | ~20B | 0 | 0 |
| `widget_steps_page_count()` | ~8B | 0 | 0 | 0 |
| `s_widget_defs[3]` (2 ptrs) | 0 | 8B | 0 | 0 |
| `app_worker_launch()` call | ~16B | 0 | 0 | 0 |
| **TOTAL** | **~244B** | **~28B** | **0B** | **0B** |

**Expected new total**: ~20,399 + 272 = ~20,671B
**Remaining**: 24,576 - 20,671 = **~3,905B** — safe margin.

### Worker (separate, does NOT count against app limits)

Worker limit: 10,752B (10.5KB)
Expected worker size: ~600-800B .text, ~16B BSS — well within limits.

---

## 8. Testing Checklist

### Build
- [ ] `pebble build` compiles without error or warning (aplite, basalt, diorite)
- [ ] `arm-none-eabi-size build/aplite/pebble-app.elf` — .text+.data+.bss delta ≤ 400B
- [ ] `build/aplite/pebble-worker.elf` exists and is < 10,752B
- [ ] `build/basalt/pebble-worker.elf` exists
- [ ] `build/diorite/pebble-worker.elf` exists

### Runtime (Emulator)
- [ ] App launches normally, watchface displays correctly
- [ ] APP_LOG from worker `prv_init` appears in logs
- [ ] After 60s, `persist_read_int(230)` returns > 0 (shake emulator)
- [ ] Navigate to Steps widget: shows step count and "steps today"
- [ ] Histogram shows all zeroes on first run (no history yet)
- [ ] Multiple widgets in list: position indicator displays correctly
- [ ] `heap_bytes_free()` value unchanged vs before (in weather request log)

### Runtime (Physical Watch)
- [ ] Worker appears in Settings → Background App
- [ ] Steps increment during normal walking
- [ ] Steps persist across app close/reopen
- [ ] After midnight: yesterday's count moves to first bar, today resets to 0
- [ ] Battery drain: measure over 24h, compare to baseline (~0.5-1%/hr increase)

### Config
- [ ] "Steps" appears in widget picker on config page
- [ ] Can add/remove Steps widget to UP/DOWN button
- [ ] Widget reflects config changes after save

---

## 9. Reference: Current Code State

### hub_config.h — HubWidgetId enum (lines 83-88)

```c
typedef enum {
  HUB_WIDGET_STOCKS = 0,
  HUB_WIDGET_WEATHER_HOURLY,
  HUB_WIDGET_WEATHER_DAILY,
  HUB_WIDGET_COUNT
} HubWidgetId;
```

### hub_widgets.c — WidgetDef struct and s_widget_defs[] (lines 39-49)

```c
typedef struct {
  void (*draw)(GContext *ctx, GRect bounds, uint8_t page);
  uint8_t (*page_count)(void);
} WidgetDef;

static const WidgetDef s_widget_defs[] = {
    [HUB_WIDGET_STOCKS] = {widget_stocks_draw, widget_stocks_page_count},
    [HUB_WIDGET_WEATHER_HOURLY] = {widget_hourly_draw,
                                   widget_hourly_page_count},
    [HUB_WIDGET_WEATHER_DAILY] = {widget_daily_draw, widget_daily_page_count},
};
```

### hub_widgets.c — File structure overview

- Lines 1-16: includes and extern declarations
- Lines 18-28: `stock_load_panel()` helper
- Lines 30-36: forward declarations of widget draw/page_count functions
- Lines 38-49: `WidgetDef` struct + `s_widget_defs[]` array
- Lines 51-67: `WidgetCtx` struct + forward declarations
- Lines 69-100: `hub_widgets_push()` entry point
- Lines 102-125: window load/unload
- Lines 127-155: `widget_update_proc()` — clears screen, calls widget draw
- Lines 156-205: click handlers and navigation
- Lines 207-222: page_count functions
- Lines 224-362: `widget_stocks_draw()` — graph, dithered fill, price labels
- Lines 364-556: `widget_hourly_draw()` — temp curve, rain bars, wind
- Lines 558-597: `widget_daily_draw()` — 2 rows per page, icons
- **END OF FILE at line 597**

### Din_Clean.c — init() function (lines 1169-1206)

```c
static void init() {
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  AppMessageResult msg_result = app_message_open(480, 32);
  s_appmsg_open = (msg_result == APP_MSG_OK);

  init_var();
  hub_config_init();
  s_main_window = window_create();
  window_stack_push(s_main_window, true);
  window_set_click_config_provider(s_main_window, click_config_provider);

  s_canvas_layer = window_get_root_layer(s_main_window);
  layer = layer_create(layer_get_bounds(s_canvas_layer));
  g_main_layer = layer;
  layer_set_update_proc(layer, update_proc);
  layer_add_child(s_canvas_layer, layer);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  bluetooth_connection_service_subscribe(bt_handler);

  app_focus_service_subscribe_handlers((AppFocusHandlers){
      .did_focus = app_focus_changed, .will_focus = app_focus_changing});
  hub_set_main_window(s_main_window);
  hub_timeout_init(hub_timeout_fired);

  // ← INSERT app_worker_launch() HERE

  s_init_done = true;
  layer_mark_dirty(layer);
}
```

### config/index.html — AVAILABLE_WIDGETS (line 1330)

```js
    var AVAILABLE_WIDGETS = [
      { id: 0, name: 'Stocks', desc: 'Configurable panels with price graph' },
      { id: 1, name: 'Hourly Weather', desc: '4 pages of 12h (48h forecast)' },
      { id: 2, name: 'Daily Weather', desc: '7-day forecast, 2 days per page' }
    ];
```

### wscript — Worker auto-detection (lines 44-59)

```python
    build_worker = os.path.exists('worker_src')
    binaries = []

    for p in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[p])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf='{}/pebble-app.elf'.format(p)
        ctx.pbl_program(source=ctx.path.ant_glob('src/c/**/*.c'),
        target=app_elf)

        if build_worker:
            worker_elf='{}/pebble-worker.elf'.format(p)
            binaries.append({'platform': p, 'app_elf': app_elf, 'worker_elf': worker_elf})
            ctx.pbl_worker(source=ctx.path.ant_glob('worker_src/c/**/*.c'),
            target=worker_elf)
        else:
            binaries.append({'platform': p, 'app_elf': app_elf})
```

---

## Summary of All Changes

| File | Action | Lines | Impact |
|------|--------|-------|--------|
| `App/src/c/hub_config.h` | Edit | Add `HUB_WIDGET_STEPS` to enum + 3 persist defines | 0B runtime |
| `App/src/c/hub_widgets.c` | Edit | Add forward decls + entry in s_widget_defs[] + `widget_steps_draw()` + `widget_steps_page_count()` at end | ~256B .text+.rodata |
| `App/src/c/Din_Clean.c` | Edit | Add `app_worker_launch()` in init() | ~16B .text |
| `App/config/index.html` | Edit | Add Steps entry to AVAILABLE_WIDGETS array | 0B on watch |
| `App/worker_src/c/step_worker.c` | **Create** | Complete background worker (~140 lines) | 10.5KB separate RAM |

**Total app binary impact: ~272B** (well under the 400B budget).
**Total heap impact: 0B**.
**Worker: self-contained** in its own 10.5KB memory space.
