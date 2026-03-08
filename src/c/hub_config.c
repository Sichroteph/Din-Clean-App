#include "hub_config.h"

HubConfig g_hub_config;

// --- Default menu items (const / in flash) ---
static const HubMenuItem s_default_down_menu[] = {
  { "Stopwatch", HUB_MI_PSEUDOAPP, -1, HUB_APP_STOPWATCH },
  { "Timer",     HUB_MI_PSEUDOAPP, -1, HUB_APP_TIMER },
  { "Alarm",     HUB_MI_PSEUDOAPP, -1, HUB_APP_ALARM },
};
#define DEFAULT_DOWN_MENU_COUNT 3

static const uint8_t s_default_up_widgets[] = { HUB_WIDGET_WEATHER };
#define DEFAULT_UP_WIDGET_COUNT 1

static const uint8_t s_default_down_widgets[] = { HUB_WIDGET_WEATHER, HUB_WIDGET_STOCKS };
#define DEFAULT_DOWN_WIDGET_COUNT 2

// --- Main window reference ---
static Window *s_main_window_ref = NULL;

// --- Timeout ---
static AppTimer *s_timeout_timer = NULL;
static HubTimeoutCallback s_timeout_cb = NULL;

static void timeout_fired(void *context) {
  s_timeout_timer = NULL;
  if (s_timeout_cb) s_timeout_cb();
}

void hub_timeout_init(HubTimeoutCallback cb) {
  s_timeout_cb = cb;
}

void hub_timeout_reset(void) {
  if (s_timeout_timer) {
    app_timer_cancel(s_timeout_timer);
    s_timeout_timer = NULL;
  }
  if (g_hub_config.timeout_s > 0 && s_timeout_cb) {
    s_timeout_timer = app_timer_register(
      (uint32_t)g_hub_config.timeout_s * 1000, timeout_fired, NULL);
  }
}

void hub_timeout_stop(void) {
  if (s_timeout_timer) {
    app_timer_cancel(s_timeout_timer);
    s_timeout_timer = NULL;
  }
}

void hub_timeout_deinit(void) {
  hub_timeout_stop();
  s_timeout_cb = NULL;
}

// --- Main window reference ---
void hub_set_main_window(Window *window) {
  s_main_window_ref = window;
}

void hub_return_to_watchface(void) {
  if (!s_main_window_ref) return;
  while (window_stack_get_top_window() != s_main_window_ref) {
    window_stack_pop(false);
  }
}

// --- Config init/load/save ---
void hub_config_init(void) {
  // Set defaults
  g_hub_config.timeout_s      = 30;
  g_hub_config.btn_up_type    = HUB_OBJ_WIDGETS;
  g_hub_config.btn_down_type  = HUB_OBJ_MENU;
  g_hub_config.lp_up_type     = HUB_LP_WEBHOOK;
  g_hub_config.lp_up_data     = 0;
  g_hub_config.lp_down_type   = HUB_LP_PSEUDOAPP;
  g_hub_config.lp_down_data   = HUB_APP_STOPWATCH;
  g_hub_config.lp_select_type = HUB_LP_PSEUDOAPP;
  g_hub_config.lp_select_data = HUB_APP_TIMER;
  g_hub_config.view_count     = HUB_VIEW_COUNT;
  for (int i = 0; i < HUB_VIEW_COUNT; i++) {
    g_hub_config.view_order[i] = i;
  }

  // Override with persisted config if available
  hub_config_load();
}

void hub_config_load(void) {
  if (persist_exists(HUB_PERSIST_CONFIG)) {
    persist_read_data(HUB_PERSIST_CONFIG, &g_hub_config, sizeof(HubConfig));
  }
}

void hub_config_save(void) {
  persist_write_data(HUB_PERSIST_CONFIG, &g_hub_config, sizeof(HubConfig));
}

// --- Data accessors ---
const HubMenuItem *hub_config_get_up_menu(uint8_t *count) {
  // By default UP = widgets, no menu
  *count = 0;
  return NULL;
}

const HubMenuItem *hub_config_get_down_menu(uint8_t *count) {
  *count = DEFAULT_DOWN_MENU_COUNT;
  return s_default_down_menu;
}

const uint8_t *hub_config_get_up_widgets(uint8_t *count) {
  *count = DEFAULT_UP_WIDGET_COUNT;
  return s_default_up_widgets;
}

const uint8_t *hub_config_get_down_widgets(uint8_t *count) {
  *count = DEFAULT_DOWN_WIDGET_COUNT;
  return s_default_down_widgets;
}

// --- Menu helper ---
uint8_t hub_menu_get_children(const HubMenuItem *items, uint8_t total,
                              int8_t parent, uint8_t *out, uint8_t max_out) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < total && n < max_out; i++) {
    if (items[i].parent_index == parent) {
      out[n++] = i;
    }
  }
  return n;
}
