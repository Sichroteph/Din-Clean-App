#include "hub_config.h"
#include <stdlib.h>

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

// --- Dynamic (custom) menu/widget storage ---
static HubMenuItem s_custom_menu_up[HUB_MAX_MENU_ITEMS];
static uint8_t s_custom_menu_up_count = 0;
static bool s_has_custom_menu_up = false;

static HubMenuItem s_custom_menu_down[HUB_MAX_MENU_ITEMS];
static uint8_t s_custom_menu_down_count = 0;
static bool s_has_custom_menu_down = false;

static uint8_t s_custom_widgets_up[HUB_MAX_WIDGETS];
static uint8_t s_custom_widgets_up_count = 0;
static bool s_has_custom_widgets_up = false;

static uint8_t s_custom_widgets_down[HUB_MAX_WIDGETS];
static uint8_t s_custom_widgets_down_count = 0;
static bool s_has_custom_widgets_down = false;

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

// --- Parse integer from bounded string ---
static int parse_int_bounded(const char *s, const char *end) {
  int result = 0;
  int sign = 1;
  if (s < end && *s == '-') { sign = -1; s++; }
  while (s < end && *s >= '0' && *s <= '9') {
    result = result * 10 + (*s - '0');
    s++;
  }
  return sign * result;
}

// --- Parse menu string: "label;type;parent;data|label2;type2;parent2;data2|..." ---
static void parse_menu_string(const char *str, HubMenuItem *out, uint8_t *count) {
  *count = 0;
  if (!str || !*str) return;

  const char *p = str;
  while (*p && *count < HUB_MAX_MENU_ITEMS) {
    // Find end of this item (next '|' or end of string)
    const char *item_end = p;
    while (*item_end && *item_end != '|') item_end++;

    // Find semicolons to split fields
    const char *fields[4];
    int fc = 0;
    fields[0] = p;
    for (const char *q = p; q < item_end && fc < 3; q++) {
      if (*q == ';') {
        fc++;
        fields[fc] = q + 1;
      }
    }

    if (fc == 3) {
      // Field 0: label (from fields[0] to first ';')
      const char *label_end = fields[1] - 1; // points at the ';'
      int label_len = label_end - fields[0];
      if (label_len >= HUB_MAX_LABEL) label_len = HUB_MAX_LABEL - 1;
      if (label_len < 0) label_len = 0;
      memcpy(out[*count].label, fields[0], label_len);
      out[*count].label[label_len] = '\0';

      // Field 1: type
      const char *f1_end = fields[2] - 1;
      out[*count].type = (uint8_t)parse_int_bounded(fields[1], f1_end);

      // Field 2: parent_index
      const char *f2_end = fields[3] - 1;
      out[*count].parent_index = (int8_t)parse_int_bounded(fields[2], f2_end);

      // Field 3: data
      out[*count].data = (uint8_t)parse_int_bounded(fields[3], item_end);

      (*count)++;
    }

    p = item_end;
    if (*p == '|') p++;
  }
}

// --- Parse widget string: "0,1,2" ---
static void parse_widget_string(const char *str, uint8_t *out, uint8_t *count) {
  *count = 0;
  if (!str || !*str) return;

  const char *p = str;
  while (*p && *count < HUB_MAX_WIDGETS) {
    int val = 0;
    bool has_digit = false;
    while (*p >= '0' && *p <= '9') {
      val = val * 10 + (*p - '0');
      has_digit = true;
      p++;
    }
    if (has_digit) {
      out[(*count)++] = (uint8_t)val;
    }
    if (*p == ',') p++;
    else if (*p && *p != ',') break;
  }
}

// --- Public parse API (called from inbox handler) ---
void hub_config_parse_menu(const char *str, bool is_up) {
  if (is_up) {
    parse_menu_string(str, s_custom_menu_up, &s_custom_menu_up_count);
    s_has_custom_menu_up = true;
    // Persist
    persist_write_data(HUB_PERSIST_MENU_UP, s_custom_menu_up,
                       s_custom_menu_up_count * sizeof(HubMenuItem));
  } else {
    parse_menu_string(str, s_custom_menu_down, &s_custom_menu_down_count);
    s_has_custom_menu_down = true;
    persist_write_data(HUB_PERSIST_MENU_DOWN, s_custom_menu_down,
                       s_custom_menu_down_count * sizeof(HubMenuItem));
  }
}

void hub_config_parse_widgets(const char *str, bool is_up) {
  if (is_up) {
    parse_widget_string(str, s_custom_widgets_up, &s_custom_widgets_up_count);
    s_has_custom_widgets_up = true;
    persist_write_data(HUB_PERSIST_WIDGETS_UP, s_custom_widgets_up,
                       s_custom_widgets_up_count);
  } else {
    parse_widget_string(str, s_custom_widgets_down, &s_custom_widgets_down_count);
    s_has_custom_widgets_down = true;
    persist_write_data(HUB_PERSIST_WIDGETS_DOWN, s_custom_widgets_down,
                       s_custom_widgets_down_count);
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

  // Load custom menu data
  if (persist_exists(HUB_PERSIST_MENU_UP)) {
    int size = persist_get_size(HUB_PERSIST_MENU_UP);
    if (size > 0 && (size % sizeof(HubMenuItem)) == 0) {
      s_custom_menu_up_count = size / sizeof(HubMenuItem);
      if (s_custom_menu_up_count > HUB_MAX_MENU_ITEMS) s_custom_menu_up_count = HUB_MAX_MENU_ITEMS;
      persist_read_data(HUB_PERSIST_MENU_UP, s_custom_menu_up,
                        s_custom_menu_up_count * sizeof(HubMenuItem));
      s_has_custom_menu_up = true;
    }
  }
  if (persist_exists(HUB_PERSIST_MENU_DOWN)) {
    int size = persist_get_size(HUB_PERSIST_MENU_DOWN);
    if (size > 0 && (size % sizeof(HubMenuItem)) == 0) {
      s_custom_menu_down_count = size / sizeof(HubMenuItem);
      if (s_custom_menu_down_count > HUB_MAX_MENU_ITEMS) s_custom_menu_down_count = HUB_MAX_MENU_ITEMS;
      persist_read_data(HUB_PERSIST_MENU_DOWN, s_custom_menu_down,
                        s_custom_menu_down_count * sizeof(HubMenuItem));
      s_has_custom_menu_down = true;
    }
  }

  // Load custom widget data
  if (persist_exists(HUB_PERSIST_WIDGETS_UP)) {
    int size = persist_get_size(HUB_PERSIST_WIDGETS_UP);
    if (size > 0) {
      s_custom_widgets_up_count = size;
      if (s_custom_widgets_up_count > HUB_MAX_WIDGETS) s_custom_widgets_up_count = HUB_MAX_WIDGETS;
      persist_read_data(HUB_PERSIST_WIDGETS_UP, s_custom_widgets_up,
                        s_custom_widgets_up_count);
      s_has_custom_widgets_up = true;
    }
  }
  if (persist_exists(HUB_PERSIST_WIDGETS_DOWN)) {
    int size = persist_get_size(HUB_PERSIST_WIDGETS_DOWN);
    if (size > 0) {
      s_custom_widgets_down_count = size;
      if (s_custom_widgets_down_count > HUB_MAX_WIDGETS) s_custom_widgets_down_count = HUB_MAX_WIDGETS;
      persist_read_data(HUB_PERSIST_WIDGETS_DOWN, s_custom_widgets_down,
                        s_custom_widgets_down_count);
      s_has_custom_widgets_down = true;
    }
  }
}

void hub_config_save(void) {
  persist_write_data(HUB_PERSIST_CONFIG, &g_hub_config, sizeof(HubConfig));
}

// --- Data accessors (return custom data if available, otherwise defaults) ---
const HubMenuItem *hub_config_get_up_menu(uint8_t *count) {
  if (s_has_custom_menu_up) {
    *count = s_custom_menu_up_count;
    return s_custom_menu_up;
  }
  *count = 0;
  return NULL;
}

const HubMenuItem *hub_config_get_down_menu(uint8_t *count) {
  if (s_has_custom_menu_down) {
    *count = s_custom_menu_down_count;
    return s_custom_menu_down;
  }
  *count = DEFAULT_DOWN_MENU_COUNT;
  return s_default_down_menu;
}

const uint8_t *hub_config_get_up_widgets(uint8_t *count) {
  if (s_has_custom_widgets_up) {
    *count = s_custom_widgets_up_count;
    return s_custom_widgets_up;
  }
  *count = DEFAULT_UP_WIDGET_COUNT;
  return s_default_up_widgets;
}

const uint8_t *hub_config_get_down_widgets(uint8_t *count) {
  if (s_has_custom_widgets_down) {
    *count = s_custom_widgets_down_count;
    return s_custom_widgets_down;
  }
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
