#include "hub_widgets.h"
#include "weather_utils.h"

// Weather data from Din_Clean.c
extern int8_t graph_temps[];
extern uint8_t graph_rains[];
extern uint8_t graph_wind_val[];
extern uint8_t graph_hours[];
extern char wind_unit_str[];
extern int8_t days_temp_v[];
extern char days_icon[][3];
extern uint8_t days_rain_v[];
extern uint8_t days_wind_v[];

// Stock data — count in RAM, panels loaded on demand from persist
extern uint8_t stock_panel_count;

// Load a single stock panel from persist into dst. Returns true on success.
static bool stock_load_panel(uint8_t idx, StockPanel *dst) {
  if (idx >= STOCK_MAX_PANELS)
    return false;
  int key = HUB_PERSIST_STOCK0 + idx;
  if (!persist_exists(key))
    return false;
  memset(dst, 0, sizeof(StockPanel));
  persist_read_data(key, dst, sizeof(StockPanel));
  return true;
}

// Active widget canvas pointer — set while the widget window is on screen
static Layer *s_active_canvas = NULL;

void hub_widget_mark_dirty(void) {
  if (s_active_canvas)
    layer_mark_dirty(s_active_canvas);
}

// --- Widget draw/page functions ---
static void widget_stocks_draw(GContext *ctx, GRect bounds, uint8_t page);
static void widget_hourly_draw(GContext *ctx, GRect bounds, uint8_t page);
static void widget_daily_draw(GContext *ctx, GRect bounds, uint8_t page);
static void widget_steps_draw(GContext *ctx, GRect bounds, uint8_t page);
static uint8_t widget_stocks_page_count(void);
static uint8_t widget_hourly_page_count(void);
static uint8_t widget_daily_page_count(void);
static uint8_t widget_steps_page_count(void);

typedef struct {
  void (*draw)(GContext *ctx, GRect bounds, uint8_t page);
  uint8_t (*page_count)(void);
} WidgetDef;

static const WidgetDef s_widget_defs[] = {
    [HUB_WIDGET_STOCKS] = {widget_stocks_draw, widget_stocks_page_count},
    [HUB_WIDGET_WEATHER_HOURLY] = {widget_hourly_draw,
                                   widget_hourly_page_count},
    [HUB_WIDGET_WEATHER_DAILY] = {widget_daily_draw, widget_daily_page_count},
    [HUB_WIDGET_STEPS] = {widget_steps_draw, widget_steps_page_count},
};

typedef struct {
  Window *window;
  Layer *canvas;
  const uint8_t *widget_ids;
  uint8_t widget_count;
  uint8_t current_index;
  uint8_t current_page;
  bool nav_up_is_next;     // true if UP = next widget
  AppTimer *refresh_timer; // 5s live refresh when Steps widget is visible
} WidgetCtx;

// Forward declarations
static void widget_update_proc(Layer *layer, GContext *ctx);
static void widget_click_config(void *context);
static void widget_up_handler(ClickRecognizerRef rec, void *context);
static void widget_down_handler(ClickRecognizerRef rec, void *context);
static void widget_select_handler(ClickRecognizerRef rec, void *context);
static void widget_window_load(Window *window);
static void widget_window_unload(Window *window);
static void widget_steps_timer_start(WidgetCtx *ctx);
static void widget_steps_timer_stop(WidgetCtx *ctx);

static bool current_is_steps(const WidgetCtx *ctx) {
  return ctx->widget_ids[ctx->current_index] == HUB_WIDGET_STEPS;
}

static void widget_steps_refresh(void *data) {
  WidgetCtx *ctx = data;
  ctx->refresh_timer = NULL;
  if (current_is_steps(ctx)) {
    layer_mark_dirty(ctx->canvas);
    ctx->refresh_timer = app_timer_register(5000, widget_steps_refresh, ctx);
  }
}

static void widget_steps_timer_start(WidgetCtx *ctx) {
  if (current_is_steps(ctx) && !ctx->refresh_timer) {
    ctx->refresh_timer = app_timer_register(5000, widget_steps_refresh, ctx);
  }
}

static void widget_steps_timer_stop(WidgetCtx *ctx) {
  if (ctx->refresh_timer) {
    app_timer_cancel(ctx->refresh_timer);
    ctx->refresh_timer = NULL;
  }
}

void hub_widgets_push(bool is_up, HubDirection direction) {
  uint8_t count;
  const uint8_t *ids;

  if (is_up) {
    ids = hub_config_get_up_widgets(&count);
  } else {
    ids = hub_config_get_down_widgets(&count);
  }

  if (!ids || count == 0)
    return;

  WidgetCtx *ctx = malloc(sizeof(WidgetCtx));
  if (!ctx)
    return;

  ctx->widget_ids = ids;
  ctx->widget_count = count;
  ctx->current_index = is_up ? (count - 1) : 0;
  ctx->current_page = 0;
  ctx->nav_up_is_next =
      is_up; // repurposed: true = UP list (exit forward/DOWN), false = DOWN
             // list (exit backward/UP)

  ctx->window = window_create();
  window_set_user_data(ctx->window, ctx);
  window_set_window_handlers(ctx->window, (WindowHandlers){
                                              .load = widget_window_load,
                                              .unload = widget_window_unload,
                                          });

  window_stack_push(ctx->window, g_hub_config.anim_enabled ? true : false);
}

static void widget_window_load(Window *window) {
  WidgetCtx *ctx = window_get_user_data(window);

  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  ctx->canvas = layer_create(bounds);
  layer_set_update_proc(ctx->canvas, widget_update_proc);
  layer_add_child(root, ctx->canvas);
  s_active_canvas = ctx->canvas;

  window_set_click_config_provider_with_context(window, widget_click_config,
                                                ctx);

  ctx->refresh_timer = NULL;
  widget_steps_timer_start(ctx);
  hub_timeout_reset();
}

static void widget_window_unload(Window *window) {
  WidgetCtx *ctx = window_get_user_data(window);
  s_active_canvas = NULL;
  widget_steps_timer_stop(ctx);
  layer_destroy(ctx->canvas);
  window_destroy(ctx->window);
  free(ctx);
}

static void widget_update_proc(Layer *layer, GContext *ctx) {
  Window *window = layer_get_window(layer);
  WidgetCtx *wctx = window_get_user_data(window);

  GRect bounds = layer_get_bounds(layer);

  // Clear background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw current widget
  uint8_t widget_id = wctx->widget_ids[wctx->current_index];
  if (widget_id < HUB_WIDGET_COUNT) {
    s_widget_defs[widget_id].draw(ctx, bounds, wctx->current_page);
  }

}

static void widget_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, widget_up_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, widget_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, widget_select_handler);
}

static void widget_navigate(WidgetCtx *ctx, bool forward) {
  hub_timeout_reset();

  if (forward) {
    if (ctx->current_index < ctx->widget_count - 1) {
      widget_steps_timer_stop(ctx);
      ctx->current_index++;
      ctx->current_page = 0;
      widget_steps_timer_start(ctx);
      layer_mark_dirty(ctx->canvas);
    } else {
      hub_return_to_watchface();
    }
  } else {
    if (ctx->current_index > 0) {
      widget_steps_timer_stop(ctx);
      ctx->current_index--;
      ctx->current_page = 0;
      widget_steps_timer_start(ctx);
      layer_mark_dirty(ctx->canvas);
    } else {
      hub_return_to_watchface();
    }
  }
}

static void widget_up_handler(ClickRecognizerRef rec, void *context) {
  WidgetCtx *ctx = context;
  widget_navigate(ctx, false); // UP always goes toward lower index
}

static void widget_down_handler(ClickRecognizerRef rec, void *context) {
  WidgetCtx *ctx = context;
  widget_navigate(ctx, true); // DOWN always goes toward higher index
}

static void widget_select_handler(ClickRecognizerRef rec, void *context) {
  WidgetCtx *ctx = context;
  hub_timeout_reset();

  uint8_t widget_id = ctx->widget_ids[ctx->current_index];
  if (widget_id < HUB_WIDGET_COUNT) {
    uint8_t pages = s_widget_defs[widget_id].page_count();
    if (pages > 1) {
      ctx->current_page = (ctx->current_page + 1) % pages;
      layer_mark_dirty(ctx->canvas);
    }
  }
}

// ========== Widget implementations (stubs) ==========

static uint8_t widget_stocks_page_count(void) {
  return stock_panel_count > 0 ? stock_panel_count : 1;
}
static uint8_t widget_hourly_page_count(void) { return 2; }
static uint8_t widget_daily_page_count(void) {
  // A day is valid if days_temp_v is not the sentinel -128
  uint8_t n = 0;
  for (uint8_t i = 0; i < 5; i++) {
    if (days_temp_v[i] == -128)
      break;
    n++;
  }
  return n > 0 ? (n + 1) / 2 : 1;
}

static void widget_stocks_draw(GContext *ctx, GRect bounds, uint8_t page) {
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);

  StockPanel panel_buf;
  if (stock_panel_count == 0 || page >= stock_panel_count ||
      !stock_load_panel(page, &panel_buf)) {
    GRect msg_rect = GRect(0, 60, bounds.size.w, 30);
    graphics_draw_text(ctx, "No stocks",
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), msg_rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
    return;
  }

  StockPanel *p = &panel_buf;
  GFont font24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont font18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont font14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  // --- Row 1: symbol (left) + price (right), both gothic24bold ---
  GRect sym_rect = GRect(2, 0, bounds.size.w - 4, 28);
  graphics_draw_text(ctx, p->symbol, font24b, sym_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);
  GRect price_rect = GRect(2, 0, bounds.size.w - 4, 28);
  graphics_draw_text(ctx, p->price, font24b, price_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
                     NULL);

  // --- Row 2: change (left, gothic18) + panel indicator (right, gothic14) ---
  GRect chg_rect = GRect(2, 30, bounds.size.w - 4, 22);
  graphics_draw_text(ctx, p->change, font18, chg_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);

  if (stock_panel_count > 1) {
    char ind[6];
    snprintf(ind, sizeof(ind), "%d/%d", page + 1, stock_panel_count);
    GRect ind_rect = GRect(bounds.size.w - 36, 34, 34, 16);
    graphics_draw_text(ctx, ind, font14, ind_rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
                       NULL);
  }

// --- Graph area ---
#define STOCK_GRAPH_TOP 54
#define STOCK_GRAPH_BOT 162
#define STOCK_GRAPH_LEFT 2
#define STOCK_GRAPH_RIGHT 142

  int graph_h = STOCK_GRAPH_BOT - STOCK_GRAPH_TOP;

  // Dotted horizontal grid (3 lines) with price labels on the right
  for (int g = 1; g <= 3; g++) {
    int gy = STOCK_GRAPH_TOP + g * graph_h / 4;
    for (int x = STOCK_GRAPH_LEFT; x < STOCK_GRAPH_RIGHT; x += 4) {
      graphics_draw_pixel(ctx, GPoint(x, gy));
    }
  }

  // Price range labels: max at top, min at bottom (right-aligned, small font)
  if (p->price_max[0] != '\0') {
    GRect max_rect = GRect(STOCK_GRAPH_LEFT, STOCK_GRAPH_TOP - 1,
                           STOCK_GRAPH_RIGHT - STOCK_GRAPH_LEFT, 14);
    graphics_draw_text(ctx, p->price_max, font14, max_rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
                       NULL);
  }
  if (p->price_min[0] != '\0') {
    GRect min_rect = GRect(STOCK_GRAPH_LEFT, STOCK_GRAPH_BOT - 14,
                           STOCK_GRAPH_RIGHT - STOCK_GRAPH_LEFT, 14);
    graphics_draw_text(ctx, p->price_min, font14, min_rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
                       NULL);
  }

  // X positions for 10 history points, evenly spaced
  int px[STOCK_HISTORY_POINTS];
  for (int i = 0; i < STOCK_HISTORY_POINTS; i++) {
    px[i] = STOCK_GRAPH_LEFT + i * (STOCK_GRAPH_RIGHT - STOCK_GRAPH_LEFT) /
                                   (STOCK_HISTORY_POINTS - 1);
  }

  // Map history (0-100) to y coordinates
  int py[STOCK_HISTORY_POINTS];
  for (int i = 0; i < STOCK_HISTORY_POINTS; i++) {
    // 100 → top, 0 → bottom
    py[i] = STOCK_GRAPH_BOT - (int)p->history[i] * graph_h / 100;
  }

  // Filled area under curve (dithered)
  for (int i = 0; i < STOCK_HISTORY_POINTS - 1; i++) {
    int x1 = px[i], x2 = px[i + 1];
    int y1 = py[i], y2 = py[i + 1];
    for (int x = x1; x <= x2; x++) {
      // Interpolate y at this x
      int dy = y1 + (y2 - y1) * (x - x1) / (x2 - x1 > 0 ? x2 - x1 : 1);
      for (int y = dy; y < STOCK_GRAPH_BOT; y++) {
        if ((x + y) % 3 == 0)
          graphics_draw_pixel(ctx, GPoint(x, y));
      }
    }
  }

  // Line graph (2px stroke)
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < STOCK_HISTORY_POINTS - 1; i++) {
    graphics_draw_line(ctx, GPoint(px[i], py[i]), GPoint(px[i + 1], py[i + 1]));
  }
  graphics_context_set_stroke_width(ctx, 1);

  // Circles at first and last points
  graphics_fill_circle(ctx, GPoint(px[0], py[0]), 3);
  graphics_fill_circle(
      ctx, GPoint(px[STOCK_HISTORY_POINTS - 1], py[STOCK_HISTORY_POINTS - 1]),
      3);
}

// ========== Hourly Weather Widget ==========

#define HOURLY_LABEL_Y 0
#define HOURLY_GRAPH_TOP 16
#define HOURLY_GRAPH_BOT 88
#define HOURLY_TEMP_Y (HOURLY_GRAPH_BOT + 2)
#define HOURLY_ICON_Y 104
#define HOURLY_WINDLABEL_Y 124
#define HOURLY_WIND_Y 140
#define HOURLY_PAGE_Y 156
#define HOURLY_MAXRAIN 40

static int hourly_temp_to_y(int8_t temp, int8_t tmin, int8_t tmax) {
  int range = tmax - tmin;
  if (range <= 0)
    range = 1;
  return HOURLY_GRAPH_TOP +
         (tmax - temp) * (HOURLY_GRAPH_BOT - HOURLY_GRAPH_TOP) / range;
}

static void widget_hourly_draw(GContext *ctx, GRect bounds, uint8_t page) {
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);

  // X positions for 5 temperature points across the screen
  static const int tx[5] = {4, 38, 72, 106, 140};

  // Select the right 5 temps: page 0 → temps[0..4], page 1 → temps[4..8]
  int8_t *temps = &graph_temps[page * 4];

  GFont font14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  // --- Hour labels ---
  for (int i = 0; i < 4; i++) {
    char hbuf[5];
    int h;
    if (page == 0) {
      h = graph_hours[i];
    } else {
      // Compute hours 12-21 from the base hour
      h = (graph_hours[0] + 12 + i * 3) % 24;
    }
    snprintf(hbuf, sizeof(hbuf), "%dh", h);
    graphics_draw_text(ctx, hbuf, font14, GRect(i * 36, HOURLY_LABEL_Y, 36, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  }

  // --- Find min/max temps for the current page ---
  int8_t ptmin = 127, ptmax = -128;
  for (int i = 0; i <= 4; i++) {
    int8_t t = temps[i];
    if (t < ptmin)
      ptmin = t;
    if (t > ptmax)
      ptmax = t;
  }
  int range = ptmax - ptmin;
  if (range < 4) {
    int pad = (4 - range + 1) / 2;
    ptmin -= pad;
    ptmax += pad;
  } else {
    ptmin -= 1;
    ptmax += 1;
  }

  // --- Dotted grid lines (drawn first, rain bars will overlay them) ---
  int y_ref_top = HOURLY_GRAPH_TOP + 8;
  int y_ref_bot = HOURLY_GRAPH_BOT - 1;
  for (int x = 0; x < bounds.size.w; x += 4) {
    graphics_draw_pixel(ctx, GPoint(x, y_ref_top));
    graphics_draw_pixel(ctx, GPoint(x, y_ref_bot));
  }
  for (int col = 1; col <= 3; col++) {
    int vx = col * 36;
    for (int y = HOURLY_GRAPH_TOP; y < HOURLY_GRAPH_BOT; y += 4) {
      graphics_draw_pixel(ctx, GPoint(vx, y));
    }
  }

  // --- Rain bars (dithered, drawn after grid so they appear on top) ---
  // Drawn just before the temperature curve so the curve remains topmost.
  for (int i = 0; i < 12; i++) {
    uint8_t rain = graph_rains[i + page * 12];
    if (rain == 0)
      continue;
    int bar_h = (int)rain * 30 / HOURLY_MAXRAIN;
    if (bar_h > 30)
      bar_h = 30;
    if (bar_h < 1)
      bar_h = 1;
    int bar_x = i * 12;
    int bar_y = HOURLY_GRAPH_BOT - bar_h;
    for (int y = bar_y; y < HOURLY_GRAPH_BOT; y++) {
      for (int x = bar_x; x < bar_x + 10 && x < bounds.size.w; x++) {
        if ((x + y) % 2 == 0)
          graphics_draw_pixel(ctx, GPoint(x, y));
      }
    }
  }

  // --- Temperature line (topmost layer in graph area) ---
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < 4; i++) {
    int y1 = hourly_temp_to_y(temps[i], ptmin, ptmax);
    int y2 = hourly_temp_to_y(temps[i + 1], ptmin, ptmax);
    graphics_draw_line(ctx, GPoint(tx[i], y1), GPoint(tx[i + 1], y2));
  }
  for (int i = 0; i <= 4; i++) {
    int y = hourly_temp_to_y(temps[i], ptmin, ptmax);
    graphics_fill_circle(ctx, GPoint(tx[i], y), 3);
  }
  graphics_context_set_stroke_width(ctx, 1);

  // --- Temperature labels (below graph, bold, aligned with hour columns) ---
  {
    GFont font18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    for (int i = 0; i < 4; i++) {
      char tbuf[6];
      snprintf(tbuf, sizeof(tbuf), "%d\xc2\xb0", temps[i]);
      graphics_draw_text(
          ctx, tbuf, font18b, GRect(i * 36, HOURLY_TEMP_Y, 36, 22),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
  }

  // --- Wind section ---
  {
    char wind_label[14];
    snprintf(wind_label, sizeof(wind_label), "Wind (%s)", wind_unit_str);
    graphics_draw_text(ctx, wind_label, font14,
                       GRect(0, HOURLY_WINDLABEL_Y, bounds.size.w, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);

    int wind_start = page * 4;
    for (int i = 0; i < 4; i++) {
      char wbuf[5];
      snprintf(wbuf, sizeof(wbuf), "%d", graph_wind_val[wind_start + i]);
      graphics_draw_text(
          ctx, wbuf, font14, GRect(i * 36, HOURLY_WIND_Y, 36, 16),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
  }

  // --- Page label ---
  const char *page_label = (page == 0) ? "0-12h" : "12-24h";
  graphics_draw_text(ctx, page_label, font14, GRect(0, HOURLY_PAGE_Y, 60, 14),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);
}

// ========== Daily Weather Widget ==========

static void draw_daily_row(GContext *ctx, int y, int day_index, int bounds_w) {
  if (day_index >= 5)
    return;
  // Skip rows with no data (sentinel -128)
  if (days_temp_v[day_index] == -128)
    return;
  graphics_context_set_text_color(ctx, GColorWhite);

  // Day abbreviation (compute from current weekday + offset)
  time_t now_t = time(NULL);
  struct tm *now_tm = localtime(&now_t);
  int wday = (now_tm->tm_wday + day_index + 1) % 7;
  const char *locale = i18n_get_system_locale();
  const char *dayn = weather_utils_get_weekday_abbrev(locale, wday);

  GFont font24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont font28b = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont font18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // Day name (left, Y-aligned with temperature)
  graphics_draw_text(ctx, dayn, font24b, GRect(2, y + 4, 42, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);

  // Weather icon (centered slightly left of screen center: center at x=63)
  int icon_id = weather_utils_build_icon(days_icon[day_index]);
  GBitmap *bmp = gbitmap_create_with_resource(icon_id);
  if (bmp) {
    graphics_draw_bitmap_in_rect(ctx, bmp, GRect(55, y + 2, 35, 35));
    gbitmap_destroy(bmp);
  }

  // Temperature (right, Y-aligned with day name)
  char tbuf[7];
  snprintf(tbuf, sizeof(tbuf), "%d\xc2\xb0", days_temp_v[day_index]);
  graphics_draw_text(ctx, tbuf, font28b, GRect(87, y + 4, 55, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
                     NULL);

  // Rain + Wind on same line below icon (larger font, centered)
  char rwbuf[16];
  const char *wu = wind_unit_str;
  const char *unit;
  if (wu[0] == 'k')
    unit = "km";
  else if (wu[0] == 'm' && wu[1] == '/')
    unit = "ms";
  else
    unit = "mp";
  snprintf(rwbuf, sizeof(rwbuf), "%dmm  %d%s", days_rain_v[day_index],
           days_wind_v[day_index], unit);
  graphics_draw_text(ctx, rwbuf, font18b, GRect(2, y + 39, bounds_w - 4, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

static void widget_daily_draw(GContext *ctx, GRect bounds, uint8_t page) {
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  int first_day = page * 2; // 0-based day index

  // Draw first day row
  if (first_day < 5) {
    draw_daily_row(ctx, 8, first_day, bounds.size.w);
  }

  // Dotted separator + second row only when two days are available
  bool has_second = (first_day + 1 < 5) && (days_temp_v[first_day + 1] != -128);
  if (has_second) {
    for (int x = 4; x < bounds.size.w - 4; x += 3) {
      graphics_draw_pixel(ctx, GPoint(x, 78));
    }
    draw_daily_row(ctx, 84, first_day + 1, bounds.size.w);
  }

  // Page label (bottom-left)
  char pbuf[16];
  GFont font14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  if (has_second) {
    snprintf(pbuf, sizeof(pbuf), "J+%d J+%d", first_day + 1, first_day + 2);
  } else {
    snprintf(pbuf, sizeof(pbuf), "J+%d", first_day + 1);
  }
  graphics_draw_text(ctx, pbuf, font14, GRect(2, bounds.size.h - 16, 80, 14),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);
}

// ========== Steps Widget ==========

static uint8_t widget_steps_page_count(void) { return 1; }

static void widget_steps_draw(GContext *ctx, GRect bounds, uint8_t page) {
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);

  // --- Today's step count ---
#ifdef PBL_HEALTH
  // basalt/diorite: HealthService gives a real-time value, no worker needed
  uint32_t today = (uint32_t)health_service_sum_today(HealthMetricStepCount);
#else
  // aplite: read from persist (written by background worker every 5s)
  uint32_t today = 0;
  if (persist_exists(HUB_PERSIST_STEPS_TODAY)) {
    today = (uint32_t)persist_read_int(HUB_PERSIST_STEPS_TODAY);
  }
#endif

#ifdef DEMO_STEPS
  today = 8432;
#endif
  // Display today's count (large, centered)
  char buf[8];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)today);
  GFont font28b = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  graphics_draw_text(ctx, buf, font28b, GRect(0, 10, bounds.size.w, 34),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);

  // Label
  GFont font14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_draw_text(
      ctx, "steps today", font14, GRect(0, 44, bounds.size.w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // --- 7-day history histogram ---
#ifdef PBL_HEALTH
  // Pre-compute today's midnight for day-boundary queries
  time_t now_hs = time(NULL);
  struct tm tm_mid = *localtime(&now_hs);
  tm_mid.tm_hour = 0;
  tm_mid.tm_min = 0;
  tm_mid.tm_sec = 0;
  time_t today_start = mktime(&tm_mid);
#endif

  uint16_t hist[7];
  uint16_t hist_max = 1; // avoid division by zero
  for (int i = 0; i < 7; i++) {
    hist[i] = 0;
#ifdef PBL_HEALTH
    time_t day_end = today_start - (time_t)i * 86400;
    time_t day_start = day_end - 86400;
    HealthValue hv =
        health_service_sum(HealthMetricStepCount, day_start, day_end);
    hist[i] = (hv > 0 && hv < 65535) ? (uint16_t)hv : 0;
#else
    int key = HUB_PERSIST_STEPS_DAY0 + i;
    if (persist_exists(key)) {
      int32_t v = persist_read_int(key);
      hist[i] = (v > 0 && v < 65535) ? (uint16_t)v : 0;
    }
#endif
    if (hist[i] > hist_max)
      hist_max = hist[i];
  }
#ifdef DEMO_STEPS
  {
    static const uint16_t d[7] = {6100, 9200, 7400, 11200, 5800, 8800, 3500};
    for (int j = 0; j < 7; j++)
      hist[j] = d[j];
    hist_max = 11200;
  }
#endif

  // Bar chart: 7 bars, bottom-aligned
  int bar_area_top = 72;
  int bar_area_bot = 148;
  int bar_h_max = bar_area_bot - bar_area_top;
  int bar_w = 14;
  int gap = (bounds.size.w - 7 * bar_w) / 8; // even spacing

  // Dotted reference lines every 5000 steps (drawn behind bars)
  graphics_context_set_stroke_color(ctx, GColorWhite);
  for (int thr = 5000; thr <= (int)hist_max; thr += 5000) {
    int y_ref = bar_area_bot - thr * bar_h_max / (int)hist_max;
    for (int px = 2; px < bounds.size.w; px += 4) {
      graphics_draw_pixel(ctx, GPoint(px, y_ref));
      graphics_draw_pixel(ctx, GPoint(px + 1, y_ref));
    }
  }

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
    graphics_draw_text(
        ctx, lbl, font14, GRect(bx - 2, bar_area_bot, bar_w + 4, 14),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}
