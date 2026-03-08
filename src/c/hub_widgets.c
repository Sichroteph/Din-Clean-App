#include "hub_widgets.h"

// --- Widget draw/page functions ---
static void widget_weather_draw(GContext *ctx, GRect bounds, uint8_t page);
static void widget_stocks_draw(GContext *ctx, GRect bounds, uint8_t page);
static uint8_t widget_weather_page_count(void);
static uint8_t widget_stocks_page_count(void);

typedef struct {
  void (*draw)(GContext *ctx, GRect bounds, uint8_t page);
  uint8_t (*page_count)(void);
  const char *name;
} WidgetDef;

static const WidgetDef s_widget_defs[] = {
  [HUB_WIDGET_WEATHER] = { widget_weather_draw, widget_weather_page_count, "Weather" },
  [HUB_WIDGET_STOCKS]  = { widget_stocks_draw,  widget_stocks_page_count,  "Stocks" },
};

typedef struct {
  Window *window;
  Layer *canvas;
  const uint8_t *widget_ids;
  uint8_t widget_count;
  uint8_t current_index;
  uint8_t current_page;
  bool nav_up_is_next; // true if UP = next widget
} WidgetCtx;

// Forward declarations
static void widget_update_proc(Layer *layer, GContext *ctx);
static void widget_click_config(void *context);
static void widget_up_handler(ClickRecognizerRef rec, void *context);
static void widget_down_handler(ClickRecognizerRef rec, void *context);
static void widget_select_handler(ClickRecognizerRef rec, void *context);
static void widget_back_handler(ClickRecognizerRef rec, void *context);
static void widget_window_load(Window *window);
static void widget_window_unload(Window *window);

void hub_widgets_push(bool is_up, HubDirection direction) {
  uint8_t count;
  const uint8_t *ids;

  if (is_up) {
    ids = hub_config_get_up_widgets(&count);
  } else {
    ids = hub_config_get_down_widgets(&count);
  }

  if (!ids || count == 0) return;

  WidgetCtx *ctx = malloc(sizeof(WidgetCtx));
  if (!ctx) return;

  ctx->widget_ids = ids;
  ctx->widget_count = count;
  ctx->current_index = 0;
  ctx->current_page = 0;
  ctx->nav_up_is_next = (direction == HUB_DIR_UP);

  ctx->window = window_create();
  window_set_user_data(ctx->window, ctx);
  window_set_window_handlers(ctx->window, (WindowHandlers) {
    .load = widget_window_load,
    .unload = widget_window_unload,
  });

  window_stack_push(ctx->window, true);
}

static void widget_window_load(Window *window) {
  WidgetCtx *ctx = window_get_user_data(window);

  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  ctx->canvas = layer_create(bounds);
  layer_set_update_proc(ctx->canvas, widget_update_proc);
  layer_add_child(root, ctx->canvas);

  window_set_click_config_provider_with_context(window, widget_click_config, ctx);

  hub_timeout_reset();
}

static void widget_window_unload(Window *window) {
  WidgetCtx *ctx = window_get_user_data(window);
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

  // Draw widget position indicator (e.g., "1/3")
  if (wctx->widget_count > 1) {
    char indicator[8];
    snprintf(indicator, sizeof(indicator), "%d/%d",
             wctx->current_index + 1, wctx->widget_count);
    graphics_context_set_text_color(ctx, GColorWhite);
    GRect ind_rect = GRect(bounds.size.w - 40, bounds.size.h - 18, 36, 16);
    graphics_draw_text(ctx, indicator,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       ind_rect, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentRight, NULL);
  }
}

static void widget_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, widget_up_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, widget_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, widget_select_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, widget_back_handler);
}

static void widget_navigate(WidgetCtx *ctx, bool forward) {
  hub_timeout_reset();

  if (forward) {
    if (ctx->current_index < ctx->widget_count - 1) {
      ctx->current_index++;
      ctx->current_page = 0;
      layer_mark_dirty(ctx->canvas);
    } else {
      // Past last widget: exit
      window_stack_pop(true);
    }
  } else {
    if (ctx->current_index > 0) {
      ctx->current_index--;
      ctx->current_page = 0;
      layer_mark_dirty(ctx->canvas);
    } else {
      // Past first widget: return to watchface
      window_stack_pop(true);
    }
  }
}

static void widget_up_handler(ClickRecognizerRef rec, void *context) {
  WidgetCtx *ctx = context;
  widget_navigate(ctx, ctx->nav_up_is_next);
}

static void widget_down_handler(ClickRecognizerRef rec, void *context) {
  WidgetCtx *ctx = context;
  widget_navigate(ctx, !ctx->nav_up_is_next);
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

static void widget_back_handler(ClickRecognizerRef rec, void *context) {
  window_stack_pop(true);
}

// ========== Widget implementations (stubs) ==========

static uint8_t widget_weather_page_count(void) { return 3; }
static uint8_t widget_stocks_page_count(void) { return 2; }

static void widget_weather_draw(GContext *ctx, GRect bounds, uint8_t page) {
  graphics_context_set_text_color(ctx, GColorWhite);

  char page_label[16];
  switch (page) {
    case 0: snprintf(page_label, sizeof(page_label), "0-12h"); break;
    case 1: snprintf(page_label, sizeof(page_label), "12-24h"); break;
    case 2: snprintf(page_label, sizeof(page_label), "24-36h"); break;
    default: snprintf(page_label, sizeof(page_label), "??"); break;
  }

  GRect title_rect = GRect(0, 40, bounds.size.w, 34);
  graphics_draw_text(ctx, "Weather",
                     fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     title_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  GRect page_rect = GRect(0, 78, bounds.size.w, 28);
  graphics_draw_text(ctx, page_label,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     page_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  GRect stub_rect = GRect(0, 112, bounds.size.w, 22);
  graphics_draw_text(ctx, "(stub)",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     stub_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void widget_stocks_draw(GContext *ctx, GRect bounds, uint8_t page) {
  graphics_context_set_text_color(ctx, GColorWhite);

  char label[16];
  snprintf(label, sizeof(label), "Stock %d", page + 1);

  GRect title_rect = GRect(0, 40, bounds.size.w, 34);
  graphics_draw_text(ctx, "Stocks",
                     fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     title_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  GRect page_rect = GRect(0, 78, bounds.size.w, 28);
  graphics_draw_text(ctx, label,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     page_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  GRect stub_rect = GRect(0, 112, bounds.size.w, 22);
  graphics_draw_text(ctx, "(stub)",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     stub_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}
