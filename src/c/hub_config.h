#pragma once
#include <pebble.h>

// Persist key for hub config binary blob
#define HUB_PERSIST_CONFIG 300

// Message keys for hub (C<->JS)
#define KEY_HUB_WEBHOOK    350
#define KEY_HUB_TIMEOUT    351
#define KEY_HUB_BTN_UP     352
#define KEY_HUB_BTN_DOWN   353
#define KEY_HUB_LP_UP      354
#define KEY_HUB_LP_DOWN    355
#define KEY_HUB_LP_SELECT  356
#define KEY_HUB_VIEWS      357

// Limits
#define HUB_MAX_MENU_ITEMS  12
#define HUB_MAX_WIDGETS      6
#define HUB_MAX_VIEWS        4
#define HUB_MAX_LABEL       16
#define HUB_MAX_MENU_DEPTH   3

// --- Enums ---
typedef enum { HUB_OBJ_MENU = 0, HUB_OBJ_WIDGETS } HubObjectType;
typedef enum { HUB_LP_PSEUDOAPP = 0, HUB_LP_WEBHOOK } HubLongPressType;
typedef enum { HUB_MI_FOLDER = 0, HUB_MI_PSEUDOAPP, HUB_MI_ACTION } HubMenuItemType;
typedef enum { HUB_APP_STOPWATCH = 0, HUB_APP_TIMER, HUB_APP_ALARM, HUB_APP_COUNT } HubPseudoAppId;
typedef enum { HUB_WIDGET_WEATHER = 0, HUB_WIDGET_STOCKS, HUB_WIDGET_COUNT } HubWidgetId;
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

// Menu helper: get children of a parent (-1 for root)
uint8_t hub_menu_get_children(const HubMenuItem *items, uint8_t total,
                              int8_t parent, uint8_t *out, uint8_t max_out);
