#pragma once
#include <pebble.h>

// Persist key for hub config binary blob
#define HUB_PERSIST_CONFIG       300
#define HUB_PERSIST_MENU_UP      301
#define HUB_PERSIST_MENU_DOWN    302
#define HUB_PERSIST_WIDGETS_UP   303
#define HUB_PERSIST_WIDGETS_DOWN 304

// Message keys for hub (C<->JS)
#define KEY_HUB_WEBHOOK    350
#define KEY_HUB_TIMEOUT    351
#define KEY_HUB_BTN_UP     352
#define KEY_HUB_BTN_DOWN   353
#define KEY_HUB_LP_UP      354
#define KEY_HUB_LP_DOWN    355
#define KEY_HUB_LP_SELECT  356
#define KEY_HUB_VIEWS      357
#define KEY_HUB_MENU_UP    358
#define KEY_HUB_MENU_DOWN  359
#define KEY_HUB_WIDGETS_UP   360
#define KEY_HUB_WIDGETS_DOWN 361
#define KEY_HUB_ANIM         362

// Stock widget message keys & persist
#define KEY_STOCK_DATA       370
#define KEY_STOCK_COUNT      371
#define HUB_PERSIST_STOCK0   310
#define HUB_PERSIST_STOCK1   311
#define HUB_PERSIST_STOCK2   312
#define HUB_PERSIST_STOCK3   313
#define HUB_PERSIST_STOCK4   314

// Stock widget limits
#define STOCK_MAX_PANELS     5
#define STOCK_HISTORY_POINTS 10

// Stock panel data (pre-formatted by JS, no float math on watch)
typedef struct {
  char symbol[10];               // Display name: "DJIA", "EUR/CHF", "BTC"
  char price[12];                // Pre-formatted: "42,531", "0.9385"
  char change[8];                // "+0.8%", "-1.2%"
  uint8_t history[STOCK_HISTORY_POINTS]; // 0-100 normalized
  bool positive;                 // true if change >= 0 (for trend indicator)
  char price_min[10];            // Pre-formatted min price of history window
  char price_max[10];            // Pre-formatted max price of history window
} StockPanel;

#ifdef PBL_PLATFORM_APLITE
// Lightweight stock panel for Aplite — no graph data, saves ~35 bytes/panel
typedef struct {
  char symbol[10];
  char price[12];
  char change[8];
  bool positive;
} StockPanelLite;
#endif

// Limits
#define HUB_MAX_MENU_ITEMS   5  // Reduced from 8 to save 90 bytes BSS on APLITE
#define HUB_MAX_WIDGETS      6
#define HUB_MAX_VIEWS        4
#define HUB_MAX_LABEL       12  // Reduced from 16 to save 64 bytes BSS on APLITE
#define HUB_MAX_MENU_DEPTH   3

// --- Enums ---
typedef enum { HUB_OBJ_MENU = 0, HUB_OBJ_WIDGETS } HubObjectType;
typedef enum { HUB_LP_PSEUDOAPP = 0, HUB_LP_WEBHOOK } HubLongPressType;
typedef enum { HUB_MI_FOLDER = 0, HUB_MI_PSEUDOAPP, HUB_MI_ACTION } HubMenuItemType;
typedef enum { HUB_APP_STOPWATCH = 0, HUB_APP_TIMER, HUB_APP_ALARM, HUB_APP_COUNT } HubPseudoAppId;
typedef enum { HUB_WIDGET_WEATHER = 0, HUB_WIDGET_STOCKS, HUB_WIDGET_WEATHER_HOURLY, HUB_WIDGET_WEATHER_DAILY, HUB_WIDGET_COUNT } HubWidgetId;
typedef enum { HUB_VIEW_MAIN = 0, HUB_VIEW_2, HUB_VIEW_3, HUB_VIEW_ANALOG, HUB_VIEW_COUNT } HubViewId;
typedef enum { HUB_DIR_UP = 0, HUB_DIR_DOWN } HubDirection;

// --- Structures ---
typedef struct {
  char label[HUB_MAX_LABEL];
  uint8_t type;         // HubMenuItemType
  int8_t  parent_index; // -1 = root level
  uint8_t data;         // pseudoapp_id or webhook_index
} HubMenuItem;

// Core config (kept in RAM, ~20 bytes)
typedef struct {
  uint16_t timeout_s;
  uint8_t  btn_up_type;     // HubObjectType
  uint8_t  btn_down_type;   // HubObjectType
  uint8_t  lp_up_type;      // HubLongPressType
  uint8_t  lp_up_data;
  uint8_t  lp_down_type;
  uint8_t  lp_down_data;
  uint8_t  lp_select_type;
  uint8_t  lp_select_data;
  uint8_t  view_count;
  uint8_t  view_order[HUB_MAX_VIEWS];
  uint8_t  anim_enabled;  // 1 = animated transitions, 0 = instant
} HubConfig;

// Global config
extern HubConfig g_hub_config;

// Timeout callback type
typedef void (*HubTimeoutCallback)(void);

// --- API ---
void hub_config_init(void);
void hub_config_load(void);
void hub_config_save(void);

// Timeout
void hub_timeout_init(HubTimeoutCallback cb);
void hub_timeout_reset(void);
void hub_timeout_stop(void);
void hub_timeout_deinit(void);

// Main window reference (for return-to-watchface)
void hub_set_main_window(Window *window);
void hub_return_to_watchface(void);

// Default menu/widget data accessors
const HubMenuItem *hub_config_get_up_menu(uint8_t *count);
const HubMenuItem *hub_config_get_down_menu(uint8_t *count);
const uint8_t     *hub_config_get_up_widgets(uint8_t *count);
const uint8_t     *hub_config_get_down_widgets(uint8_t *count);

// Parse incoming menu/widget config strings from JS
void hub_config_parse_menu(const char *str, bool is_up);
void hub_config_parse_widgets(const char *str, bool is_up);

// Menu helper: get children of a parent (-1 for root)
uint8_t hub_menu_get_children(const HubMenuItem *items, uint8_t total,
                              int8_t parent, uint8_t *out, uint8_t max_out);
