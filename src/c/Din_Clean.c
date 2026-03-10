#include <inttypes.h>
#include <pebble.h>
#include <stdbool.h>

#include "hub_actions.h"
#include "hub_config.h"
#include "hub_menu.h"
#include "hub_pseudoapp.h"
#include "hub_widgets.h"
#include "ui_icon_bar.h"
#include "ui_time.h"
#include "weather_utils.h"

#define FORCE_REFRESH false
#define IS_HOUR_FICTIVE false

#define FICTIVE_HOUR 12
#define FICTIVE_MINUTE 34

#define RULER_XOFFSET 36
#define TEXT_HOUR_OFFSET 56

#define MARK_0 42

#define RULER_SIZE -2
#define MAXRAIN 40

#define LINE_THICK 3

#define GRADIANT 4
#define GRADIANT_X_OFFSET -1
#define GRADIANT_Y_OFFSET -10

// Unified B&W mode for all platforms (aplite style)
#define BLUE_LINE GColorWhite
#define RAIN_COLOR GColorWhite
#define RED_LINE GColorWhite
#define IS_COLOR false

#define STATUS_FONT FONT_KEY_GOTHIC_14

#define KEY_TEMPERATURE 0
#define KEY_WIND_SPEED 3
#define KEY_HUMIDITY 5
#define KEY_TMIN 8
#define KEY_TMAX 9
#define KEY_ICON 10

#define KEY_FORECAST_TEMP1 11
#define KEY_FORECAST_TEMP2 12
#define KEY_FORECAST_TEMP3 13
#define KEY_FORECAST_TEMP4 14
#define KEY_FORECAST_TEMP5 15
#define KEY_FORECAST_H1 16
#define KEY_FORECAST_H2 17
#define KEY_FORECAST_H3 18
#define KEY_FORECAST_RAIN1 21
#define KEY_FORECAST_RAIN2 22
#define KEY_FORECAST_RAIN3 23
#define KEY_FORECAST_RAIN4 24
#define KEY_FORECAST_ICON1 26
#define KEY_FORECAST_ICON2 27
#define KEY_FORECAST_ICON3 28
#define KEY_FORECAST_WIND1 29
#define KEY_FORECAST_WIND2 30
#define KEY_FORECAST_WIND3 31
#define KEY_LOCATION 32

#define KEY_FORECAST_H0 120
#define KEY_FORECAST_WIND0 149

// Detailed rain keys (3 per 3-hour block)
#define KEY_FORECAST_RAIN11 126
#define KEY_FORECAST_RAIN12 127
#define KEY_FORECAST_RAIN21 128
#define KEY_FORECAST_RAIN22 129
#define KEY_FORECAST_RAIN31 130
#define KEY_FORECAST_RAIN32 131
#define KEY_FORECAST_RAIN41 132
#define KEY_FORECAST_RAIN42 133

// Extended hourly forecast: temps for h+15, h+18, h+21, h+24
#define KEY_FORECAST_TEMP6 212
#define KEY_FORECAST_TEMP7 213
#define KEY_FORECAST_TEMP8 214
#define KEY_FORECAST_TEMP9 215

// Extended hourly winds for page 2 (h+12, h+15, h+18, h+21)
#define KEY_FORECAST_WIND4 224
#define KEY_FORECAST_WIND5 225
#define KEY_FORECAST_WIND6 226
#define KEY_FORECAST_WIND7 227

#define KEY_RADIO_UNITS 36
#define KEY_RADIO_REFRESH 54
#define KEY_TOGGLE_VIBRATION 37
#define KEY_COLOR_RIGHT_R 39
#define KEY_COLOR_RIGHT_G 40
#define KEY_COLOR_RIGHT_B 41
#define KEY_COLOR_LEFT_R 42
#define KEY_COLOR_LEFT_G 43
#define KEY_COLOR_LEFT_B 44

#define KEY_TOGGLE_BT 103
#define KEY_LAST_REFRESH 55

#define KEY_POOLTEMP 113
#define KEY_POOLPH 114
#define KEY_poolORP 115

// 3-day forecast keys
#define KEY_DAY1_TEMP 200
#define KEY_DAY1_ICON 201
#define KEY_DAY1_RAIN 202
#define KEY_DAY1_WIND 203
#define KEY_DAY2_TEMP 204
#define KEY_DAY2_ICON 205
#define KEY_DAY2_RAIN 206
#define KEY_DAY2_WIND 207
#define KEY_DAY3_TEMP 208
#define KEY_DAY3_ICON 209
#define KEY_DAY3_RAIN 210
#define KEY_DAY3_WIND 211
#define KEY_DAY4_TEMP 216
#define KEY_DAY4_ICON 217
#define KEY_DAY4_RAIN 218
#define KEY_DAY4_WIND 219
#define KEY_DAY5_TEMP 220
#define KEY_DAY5_ICON 221
#define KEY_DAY5_RAIN 222
#define KEY_DAY5_WIND 223

//******************************************************************

#define WIDTH 144
#define HEIGHT 168

#define XOFFSET 18
#define YOFFSET 0

#define TEXT_DAYW_STATUS_OFFSET_X -3
#define TEXT_DAYW_STATUS_OFFSET_Y 0

#define TEXT_DAY_STATUS_OFFSET_X -1
#define TEXT_DAY_STATUS_OFFSET_Y 6
#define TEXT_MONTH_STATUS_OFFSET_X -3
#define TEXT_MONTH_STATUS_OFFSET_Y 32

#define ICON_BT_X -2
#define ICON_BT_Y 16

#define BAT_STATUS_OFFSET_X 7

#define BAT_STATUS_OFFSET_Y 127
#define ICONW_X 3
#define ICONW_Y 77
#define BAT_PHONE_STATUS_OFFSET_X 0
#define BAT_PHONE_STATUS_OFFSET_Y 57

#define ICON_X 0
#define ICON_Y 30

#define TEXT_TEMP_OFFSET_X -12
#define TEXT_TEMP_OFFSET_Y 75

#define TEXT_HUM_OFFSET_X 2
#define TEXT_HUM_OFFSET_Y 77

#define TEXT_TMIN_OFFSET_X -5
#define TEXT_TMIN_OFFSET_Y 131
#define TEXT_TMAX_OFFSET_X -5
#define TEXT_TMAX_OFFSET_Y 144

#define ICON6_X 0
#define ICON6_Y 101

#define TEXT_WIND_OFFSET_X 10
#define TEXT_WIND_OFFSET_Y 80

#define BT_STATUS_OFFSET_Y -27

#define WEATHER_OFFSET_X -14
#define WEATHER_OFFSET_Y -9

#define MARK_5 12
#define MARK_15 22
#define MARK_30 32

#define QUIET_TIME_START 22
#define QUIET_TIME_END 10

static Window *s_main_window;
static Layer *s_canvas_layer;
static Layer *layer;

static char week_day[4] = " ";
static char mday[4] = " ";
static char weather_temp_char[8] = " ";
static char minTemp[6] = " ";
static char maxTemp[6] = " ";

// POOL DATA (numeric only — string formatting removed to save heap)
int npoolTemp;
int npoolPH;
int npoolORP;

// WEATHER
static int8_t weather_temp = 0;

static time_t t;
static struct tm now;

int8_t tmin_val = 0;
int8_t tmax_val = 0;
uint8_t wind_speed_val = 0;
uint8_t humidity = 0;
time_t last_refresh = 0;
int duration = 3600;
int offline_delay = 3600;

static char icon[20] = " ";
static char icon1[20] = " ";
static char icon2[20] = " ";
static char icon3[20] = " ";

static char location[16] = " ";
static char rain_ico_val;

// Hourly forecast data (9 temps for 0-24h, 12 rain bars, 8 winds for 0-24h, 4
// hours for 0-12h)
int8_t graph_temps[9] = {10, 10, 10, 10, 10, 10, 10, 10, 10};
uint8_t graph_rains[12] = {0};
uint8_t graph_wind_val[8] = {0};
uint8_t graph_hours[4] = {0, 3, 6, 9};

// 3-day forecast data
char days_temp[5][8] = {"--", "--", "--", "--", "--"};
char days_icon[5][20] = {"", "", "", "", ""};
char days_rain[5][5] = {"0mm", "0mm", "0mm", "0mm", "0mm"};
char days_wind[5][5] = {"0km", "0km", "0km", "0km", "0km"};
char wind_unit_str[6] = "km/h";

// Stock widget data count (panels stored in persist, loaded on demand)
uint8_t stock_panel_count = 0;

// Weather retry protection
static bool s_weather_request_pending = false;
static AppTimer *s_weather_retry_timer = NULL;

// AppMessage guard: true only when app_message_open() succeeded
static bool s_appmsg_open = false;
// init guard: true after init() completes (app_event_loop not yet started when
// false)
static bool s_init_done = false;
#define WEATHER_RETRY_DELAY_MS 5000

// Hub: current watchface view index
static uint8_t current_view_index = 0;

// Packed flags to save memory
typedef struct {
  uint16_t is_metric : 1;
  uint16_t is_30mn : 1;
  uint16_t is_bt : 1;
  uint16_t is_vibration : 1;
  uint16_t is_charging : 1;
  uint16_t is_connected : 1;
} AppFlags;
static AppFlags flags = {.is_metric = 1,
                         .is_30mn = 1,
                         .is_bt = 0,
                         .is_vibration = 0,
                         .is_charging = 0,
                         .is_connected = 0};

static GColor color_right;
static GColor color_left;
static GColor color_temp;

static int8_t hour_offset_x = 0;
static int8_t hour_offset_y = 0;
static int8_t status_offset_x = 0;
static int8_t status_offset_y = 0;

static GFont fontsmall;
static GFont fontsmallbold;
static GFont fontmedium;

static void app_focus_changed(bool focused) {
  if (focused) {
    layer_set_hidden(layer, false);
    layer_mark_dirty(layer);

    // Check if weather data is stale and request refresh
    t = time(NULL);
    now = *(localtime(&t));
    if (s_init_done && s_appmsg_open && (flags.is_connected) &&
        ((mktime(&now) - last_refresh) > duration)) {
      s_weather_request_pending = true;

      DictionaryIterator *iter;
      AppMessageResult result = app_message_outbox_begin(&iter);
      if (result == APP_MSG_OK) {
        dict_write_uint8(iter, 0, 0);
        result = app_message_outbox_send();
        if (result == APP_MSG_OK) {
          s_weather_request_pending = false;
        }
      }
    }
  }
}

static void app_focus_changing(bool focusing) {
  if (focusing) {
    layer_set_hidden(layer, true);
  }
}

// Wrapper around weather_utils_build_icon with pool warning check
static int build_icon_with_pool_check(const char *text_icon) {
  if (npoolORP != 0) {
    if ((npoolORP < 650) || (npoolPH < 710)) {
      return RESOURCE_ID_WARNING_W;
    }
  }
  return weather_utils_build_icon(text_icon, true);
}

static void update_proc(Layer *layer, GContext *ctx) {
  // Static locals for large structs to avoid stack overflow on real hardware
  // (Pebble watch has ~2KB app stack; IconBarData/TimeRenderData would
  // overflow).
  static TimeRenderData time_data;
  static IconBarData icon_data;

  if (!s_init_done) {
    // Called during window_stack_push before init() finishes — skip drawing.
    return;
  }

  // Skip rendering when a hub window is on top (menu/widgets/actions) —
  // avoids loading bitmaps that would be hidden anyway.
  if (window_stack_get_top_window() != s_main_window) {
    return;
  }

  APP_LOG(APP_LOG_LEVEL_INFO, "HEAP pre-render: %zu", heap_bytes_free());

  // DRAW DIAL
  icon_data.rect_text_day =
      (GRect){{TEXT_DAY_STATUS_OFFSET_X + status_offset_x,
               TEXT_DAY_STATUS_OFFSET_Y + status_offset_y},
              {RULER_XOFFSET, 150}};
  icon_data.rect_text_dayw =
      (GRect){{TEXT_DAYW_STATUS_OFFSET_X + status_offset_x,
               TEXT_DAYW_STATUS_OFFSET_Y + status_offset_y},
              {RULER_XOFFSET, 150}};
  icon_data.rect_temp = (GRect){{TEXT_TEMP_OFFSET_X + status_offset_x,
                                 TEXT_TEMP_OFFSET_Y + status_offset_y},
                                {60, 60}};

  icon_data.rect_tmin = (GRect){
      {TEXT_TMIN_OFFSET_X, TEXT_TMIN_OFFSET_Y + status_offset_y}, {45, 35}};
  icon_data.rect_tmax = (GRect){
      {TEXT_TMAX_OFFSET_X, TEXT_TMAX_OFFSET_Y + status_offset_y}, {45, 35}};

  icon_data.rect_screen = (GRect){{0, 0}, {144, 168}};

  icon_data.rect_icon = (GRect){{ICON_X, ICON_Y + 9}, {35, 35}};
  icon_data.rect_icon6 = (GRect){{ICON6_X, ICON6_Y + 9}, {35, 35}};

  icon_data.rect_icon_hum1 = (GRect){{5, 116}, {7, 10}};
  icon_data.rect_icon_hum2 = (GRect){{14, 116}, {7, 10}};
  icon_data.rect_icon_hum3 = (GRect){{23, 116}, {7, 10}};
  icon_data.rect_icon_leaf = (GRect){{12, 116}, {11, 10}};

  icon_data.rect_bt_disconect = (GRect){
      {ICON_BT_X + status_offset_x, ICON_BT_Y + status_offset_y}, {35, 17}};

  int icon_id;
  int icon_id6;
  rain_ico_val = graph_rains[0];
  icon_id = build_icon_with_pool_check(icon);
  rain_ico_val = graph_rains[6];
  icon_id6 = build_icon_with_pool_check(icon2);

  // Draw background
  graphics_context_set_fill_color(ctx, color_right);
  graphics_fill_rect(ctx, GRect(RULER_XOFFSET, 0, 160, 180), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, color_left);
  graphics_fill_rect(ctx, GRect(0, 0, RULER_XOFFSET, 180), 0, GCornerNone);

  t = time(NULL);
  now = *(localtime(&t));

  const char *locale = i18n_get_system_locale();
  snprintf(week_day, sizeof(week_day), "%s",
           weather_utils_get_weekday_abbrev(locale, now.tm_wday));

  snprintf(mday, sizeof(mday), "%i", now.tm_mday);
  graphics_context_set_text_color(ctx, color_temp);
  icon_data.rect_text_dayw.origin.x += 2;

  bool has_fresh_weather =
      ((mktime(&now) - last_refresh) < duration + offline_delay);

  snprintf(weather_temp_char, sizeof(weather_temp_char), "%i°", weather_temp);
  snprintf(minTemp, sizeof(minTemp), "%i°", tmin_val);
  snprintf(maxTemp, sizeof(maxTemp), "%i°", tmax_val);

  // Draw hours FIRST for instant display (only 4 small bitmaps)
  static char heure[10];
  t = time(NULL);
  now = *(localtime(&t));

  if (clock_is_24h_style() == true) {
    strftime(heure, sizeof(heure), "%H%M", &now);
  } else {
    strftime(heure, sizeof(heure), "%I%M", &now);
  }

  if (IS_HOUR_FICTIVE) {
    snprintf(heure, sizeof(heure), "%i%i", FICTIVE_HOUR, FICTIVE_MINUTE);
  }

  int offset_x = 41;
  int offset_y = 85;
  int num_x = 60;
  int num_y = 1;

  time_data.digit_rects[0] = (GRect){{num_x, num_y}, {46, 81}};
  time_data.digit_rects[1] = (GRect){{num_x + offset_x, num_y}, {46, 81}};
  time_data.digit_rects[2] = (GRect){{num_x, num_y + offset_y}, {46, 81}};
  time_data.digit_rects[3] =
      (GRect){{num_x + offset_x, num_y + offset_y}, {46, 81}};
  snprintf(time_data.digits, sizeof(time_data.digits), "%s", heure);
  ui_draw_time(ctx, &time_data);

  // Draw icon bar AFTER time (loads many bitmaps, slower)
  icon_data.fontsmall = fontsmall;
  icon_data.fontsmallbold = fontsmallbold;
  icon_data.fontmedium = fontmedium;
  icon_data.color_temp = color_temp;
  icon_data.week_day = week_day;
  icon_data.mday = mday;
  icon_data.min_temp_text = minTemp;
  icon_data.max_temp_text = maxTemp;
  icon_data.weather_temp_text = weather_temp_char;
  icon_data.has_fresh_weather = has_fresh_weather;
  icon_data.is_connected = flags.is_connected;
  icon_data.is_quiet_time = quiet_time_is_active();
  icon_data.is_bw_icon = true;
  icon_data.is_metric = flags.is_metric;
  icon_data.humidity = humidity;
  icon_data.wind_speed_val = wind_speed_val;
  icon_data.wind2_val = graph_wind_val[2];
  icon_data.met_unit = flags.is_metric ? 20 : 25;
  icon_data.icon_id = icon_id;
  icon_data.icon_id6 = icon_id6;
  ui_draw_icon_bar(ctx, &icon_data);

  APP_LOG(APP_LOG_LEVEL_INFO, "HEAP post-render: %zu", heap_bytes_free());
  // Draw view label for non-main views (stubs)
  if (g_hub_config.view_count > 0 &&
      current_view_index < g_hub_config.view_count) {
    uint8_t view_id = g_hub_config.view_order[current_view_index];
    const char *view_label = NULL;
    switch (view_id) {
    case HUB_VIEW_2:
      view_label = "View 2";
      break;
    case HUB_VIEW_3:
      view_label = "View 3";
      break;
    case HUB_VIEW_ANALOG:
      view_label = "Analog";
      break;
    default:
      break;
    }
    if (view_label) {
      graphics_context_set_text_color(ctx, GColorWhite);
      GRect label_rect = GRect(40, 0, 100, 20);
      graphics_draw_text(ctx, view_label,
                         fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                         label_rect, GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentRight, NULL);
    }
  }
}

static void handle_tick(struct tm *cur, TimeUnits units_changed) {
  t = time(NULL);
  now = *(localtime(&t));
  if (flags.is_vibration) {
    if (now.tm_min == 0 && now.tm_hour >= QUIET_TIME_END &&
        now.tm_hour <= QUIET_TIME_START) {
      vibes_double_pulse();
    }
  }

  // Get weather update every 30 minutes (even during quiet time)
  if (s_init_done && s_appmsg_open && flags.is_connected) {
    if ((((flags.is_30mn) && (now.tm_min % 30 == 0)) ||
         (now.tm_min % 60 == 0) ||
         ((mktime(&now) - last_refresh) > duration))) {
      DictionaryIterator *iter;
      AppMessageResult result = app_message_outbox_begin(&iter);
      if (result == APP_MSG_OK) {
        dict_write_uint8(iter, 0, 0);
        result = app_message_outbox_send();
        if (result == APP_MSG_OK) {
          s_weather_request_pending = false;
        } else {
          s_weather_request_pending = true;
        }
      } else {
        s_weather_request_pending = true;
      }
    }
  }

  layer_mark_dirty(layer);
}

void bt_handler(bool connected) {
  if (connected) {
    flags.is_connected = true;
  } else {
    flags.is_connected = false;
  }
  if (flags.is_bt) {
    vibes_double_pulse();
  }
  layer_mark_dirty(layer);
}

static void assign_fonts() {
  fontsmall = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  fontsmallbold = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  fontmedium = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  hour_offset_x = 1;
  hour_offset_y = 9;
  status_offset_x = 1;
  status_offset_y = 0;
}

static void inbox_received_callback(DictionaryIterator *iterator,
                                    void *context) {
  // Read tuples for data
  Tuple *radio_tuple = dict_find(iterator, KEY_RADIO_UNITS);
  Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);

  APP_LOG(APP_LOG_LEVEL_INFO,
          "INBOX: radio=%s(%d) temp=%s hub_tmo=%s wgt_up=%s",
          radio_tuple ? "OK" : "NULL",
          radio_tuple ? (int)radio_tuple->value->int32 : -1,
          temp_tuple ? "OK" : "NULL",
          dict_find(iterator, KEY_HUB_TIMEOUT) ? "OK" : "NULL",
          dict_find(iterator, KEY_HUB_WIDGETS_UP) ? "OK" : "NULL");

  // If all data is available, use it
  if (temp_tuple) {

    Tuple *wind_speed_tuple = dict_find(iterator, KEY_WIND_SPEED);
    Tuple *humidity_tuple = dict_find(iterator, KEY_HUMIDITY);
    Tuple *tmin_tuple = dict_find(iterator, KEY_TMIN);
    Tuple *tmax_tuple = dict_find(iterator, KEY_TMAX);
    Tuple *icon_tuple = dict_find(iterator, KEY_ICON);

    // Pool tuples
    Tuple *poolTemp_tuple = dict_find(iterator, KEY_POOLTEMP);
    Tuple *poolPH_tuple = dict_find(iterator, KEY_POOLPH);
    Tuple *poolORP_tuple = dict_find(iterator, KEY_poolORP);

    // Pool data
    if (poolTemp_tuple)
      npoolTemp = (int)poolTemp_tuple->value->int32;
    if (poolPH_tuple)
      npoolPH = (int)poolPH_tuple->value->int32;
    if (poolORP_tuple)
      npoolORP = (int)poolORP_tuple->value->int32;

    snprintf(icon, sizeof(icon), "%s", icon_tuple->value->cstring);
    weather_temp = (int)temp_tuple->value->int32;
    tmin_val = (int)tmin_tuple->value->int32;
    tmax_val = (int)tmax_tuple->value->int32;
    wind_speed_val = (int)wind_speed_tuple->value->int32;
    humidity = (int)humidity_tuple->value->int32;

    // Hourly temps, hours, rain, icons, winds — compact loops
    Tuple *t;
    for (int i = 0; i < 5; i++)
      if ((t = dict_find(iterator, KEY_FORECAST_TEMP1 + i)))
        graph_temps[i] = (int)t->value->int32;

    if ((t = dict_find(iterator, KEY_FORECAST_H0)))
      graph_hours[0] = (int)t->value->int32;
    for (int i = 0; i < 3; i++)
      if ((t = dict_find(iterator, KEY_FORECAST_H1 + i)))
        graph_hours[1 + i] = (int)t->value->int32;

    for (int b = 0; b < 4; b++) {
      if ((t = dict_find(iterator, KEY_FORECAST_RAIN1 + b)))
        graph_rains[b * 3] = (int)t->value->int32;
      if ((t = dict_find(iterator, KEY_FORECAST_RAIN11 + b * 2)))
        graph_rains[b * 3 + 1] = (int)t->value->int32;
      if ((t = dict_find(iterator, KEY_FORECAST_RAIN11 + b * 2 + 1)))
        graph_rains[b * 3 + 2] = (int)t->value->int32;
    }

    char *icon_ptrs[] = {icon1, icon2, icon3};
    for (int i = 0; i < 3; i++)
      if ((t = dict_find(iterator, KEY_FORECAST_ICON1 + i)))
        snprintf(icon_ptrs[i], sizeof(icon1), "%s", t->value->cstring);

    if ((t = dict_find(iterator, KEY_FORECAST_WIND0)))
      graph_wind_val[0] = atoi(t->value->cstring);
    for (int i = 0; i < 3; i++)
      if ((t = dict_find(iterator, KEY_FORECAST_WIND1 + i)))
        graph_wind_val[1 + i] = atoi(t->value->cstring);

    last_refresh = mktime(&now);

    // Persist core weather
    persist_write_string(KEY_ICON, icon);
    persist_write_int(KEY_LAST_REFRESH, last_refresh);
    persist_write_int(KEY_TEMPERATURE, weather_temp);
    persist_write_int(KEY_WIND_SPEED, wind_speed_val);
    persist_write_int(KEY_HUMIDITY, humidity);
    persist_write_int(KEY_TMIN, tmin_val);
    persist_write_int(KEY_TMAX, tmax_val);
    persist_write_int(KEY_FORECAST_H0, graph_hours[0]);
    persist_write_int(KEY_POOLTEMP, npoolTemp);
    persist_write_int(KEY_POOLPH, npoolPH);
    persist_write_int(KEY_poolORP, npoolORP);
    persist_write_data(KEY_FORECAST_TEMP1, graph_temps, sizeof(graph_temps));
    persist_write_data(KEY_FORECAST_RAIN1, graph_rains, sizeof(graph_rains));
    persist_write_data(KEY_FORECAST_WIND1, graph_wind_val,
                       sizeof(graph_wind_val));
    persist_write_data(KEY_FORECAST_H1, graph_hours, sizeof(graph_hours));

    layer_mark_dirty(layer);
  }

  // 3-day forecast message (dict2 from JS — no KEY_TEMPERATURE present)
  Tuple *icon1_check = dict_find(iterator, KEY_FORECAST_ICON1);
  if (!temp_tuple && icon1_check) {
    Tuple *t;
    char *ic_ptrs[] = {icon1, icon2, icon3};
    for (int i = 0; i < 3; i++)
      if ((t = dict_find(iterator, KEY_FORECAST_ICON1 + i)))
        snprintf(ic_ptrs[i], sizeof(icon1), "%s", t->value->cstring);
    // 5-day forecast: days 1-3 base=200+d*4, days 4-5 base=204+d*4
    for (int d = 0; d < 5; d++) {
      uint32_t base = (d < 3 ? 200 : 204) + d * 4;
      if ((t = dict_find(iterator, base)))
        snprintf(days_temp[d], sizeof(days_temp[d]), "%s", t->value->cstring);
      if ((t = dict_find(iterator, base + 1)))
        snprintf(days_icon[d], sizeof(days_icon[d]), "%s", t->value->cstring);
      if ((t = dict_find(iterator, base + 2)))
        snprintf(days_rain[d], sizeof(days_rain[d]), "%s", t->value->cstring);
      if ((t = dict_find(iterator, base + 3))) {
        snprintf(days_wind[d], sizeof(days_wind[d]), "%s", t->value->cstring);
        if (d == 0) {
          const char *p = t->value->cstring;
          while (*p >= '0' && *p <= '9')
            p++;
          if (strncmp(p, "m/s", 3) == 0)
            snprintf(wind_unit_str, sizeof(wind_unit_str), "m/s");
          else if (strncmp(p, "mph", 3) == 0)
            snprintf(wind_unit_str, sizeof(wind_unit_str), "mph");
          else
            snprintf(wind_unit_str, sizeof(wind_unit_str), "km/h");
        }
      }
    }

    // Extended hourly temps + winds (page 2)
    for (int i = 0; i < 4; i++) {
      if ((t = dict_find(iterator, KEY_FORECAST_TEMP6 + i)))
        graph_temps[5 + i] = (int)t->value->int32;
      if ((t = dict_find(iterator, KEY_FORECAST_WIND4 + i)))
        graph_wind_val[4 + i] = atoi(t->value->cstring);
    }

    persist_write_string(KEY_FORECAST_ICON1, icon1);
    persist_write_string(KEY_FORECAST_ICON2, icon2);
    persist_write_string(KEY_FORECAST_ICON3, icon3);
    persist_write_data(KEY_DAY1_TEMP, days_temp, sizeof(days_temp));
    persist_write_data(KEY_DAY1_ICON, days_icon, sizeof(days_icon));
    persist_write_data(KEY_DAY1_RAIN, days_rain, sizeof(days_rain));
    persist_write_data(KEY_DAY1_WIND, days_wind, sizeof(days_wind));

    layer_mark_dirty(layer);
  }

  // Configuration message received
  if (radio_tuple) {
    Tuple *refresh_tuple = dict_find(iterator, KEY_RADIO_REFRESH);
    Tuple *vibration_tuple = dict_find(iterator, KEY_TOGGLE_VIBRATION);
    Tuple *bt_tuple = dict_find(iterator, KEY_TOGGLE_BT);

    Tuple *color_right_r_tuple = dict_find(iterator, KEY_COLOR_RIGHT_R);
    Tuple *color_right_g_tuple = dict_find(iterator, KEY_COLOR_RIGHT_G);
    Tuple *color_right_b_tuple = dict_find(iterator, KEY_COLOR_RIGHT_B);

    Tuple *color_left_r_tuple = dict_find(iterator, KEY_COLOR_LEFT_R);
    Tuple *color_left_g_tuple = dict_find(iterator, KEY_COLOR_LEFT_G);
    Tuple *color_left_b_tuple = dict_find(iterator, KEY_COLOR_LEFT_B);

    int red;
    int green;
    int blue;

    flags.is_bt = bt_tuple ? (bt_tuple->value->int32 == 1) : flags.is_bt;
    flags.is_metric =
        radio_tuple ? (radio_tuple->value->int32 != 1) : flags.is_metric;
    flags.is_30mn =
        refresh_tuple ? (refresh_tuple->value->int32 == 1) : flags.is_30mn;
    flags.is_vibration = vibration_tuple ? (vibration_tuple->value->int32 == 1)
                                         : flags.is_vibration;

    if (color_right_r_tuple && color_right_g_tuple && color_right_b_tuple) {
      red = color_right_r_tuple->value->int32;
      green = color_right_g_tuple->value->int32;
      blue = color_right_b_tuple->value->int32;
      persist_write_int(KEY_COLOR_RIGHT_R, red);
      persist_write_int(KEY_COLOR_RIGHT_G, green);
      persist_write_int(KEY_COLOR_RIGHT_B, blue);
      color_right = GColorFromRGB(red, green, blue);
    }

    if (color_left_r_tuple && color_left_g_tuple && color_left_b_tuple) {
      red = color_left_r_tuple->value->int32;
      green = color_left_g_tuple->value->int32;
      blue = color_left_b_tuple->value->int32;
      persist_write_int(KEY_COLOR_LEFT_R, red);
      persist_write_int(KEY_COLOR_LEFT_G, green);
      persist_write_int(KEY_COLOR_LEFT_B, blue);
      color_left = GColorFromRGB(red, green, blue);
    }

    // Force B&W colors for all platforms (aplite style)
    color_left = GColorBlack;
    color_right = GColorBlack;

    persist_write_bool(KEY_RADIO_UNITS, flags.is_metric);
    persist_write_bool(KEY_RADIO_REFRESH, flags.is_30mn);
    persist_write_bool(KEY_TOGGLE_BT, flags.is_bt);
    persist_write_bool(KEY_TOGGLE_VIBRATION, flags.is_vibration);

    APP_LOG(APP_LOG_LEVEL_INFO, "CONFIG: applying, is_metric=%d, vibrating",
            (int)flags.is_metric);
    light_enable_interaction(); // visual confirmation: backlight flashes on
                                // config receive
    vibes_double_pulse();

    // Request immediate weather update to apply new units
    if (s_appmsg_open) {
      DictionaryIterator *iter;
      AppMessageResult outbox_result = app_message_outbox_begin(&iter);
      if (outbox_result == APP_MSG_OK) {
        dict_write_uint8(iter, 0, 0);
        app_message_outbox_send();
      }
    }
  }

  // Hub config message (sent alongside regular config, or separately)
  Tuple *hub_timeout_tuple = dict_find(iterator, KEY_HUB_TIMEOUT);
  if (hub_timeout_tuple) {
    g_hub_config.timeout_s = (uint16_t)hub_timeout_tuple->value->int32;

    Tuple *t;
    if ((t = dict_find(iterator, KEY_HUB_BTN_UP)))
      g_hub_config.btn_up_type =
          (t->value->int32 == 1) ? HUB_OBJ_WIDGETS : HUB_OBJ_MENU;
    if ((t = dict_find(iterator, KEY_HUB_BTN_DOWN)))
      g_hub_config.btn_down_type =
          (t->value->int32 == 1) ? HUB_OBJ_WIDGETS : HUB_OBJ_MENU;
    if ((t = dict_find(iterator, KEY_HUB_LP_UP))) {
      uint8_t v = (uint8_t)t->value->int32;
      g_hub_config.lp_up_type = v >> 4;
      g_hub_config.lp_up_data = v & 0x0F;
    }
    if ((t = dict_find(iterator, KEY_HUB_LP_DOWN))) {
      uint8_t v = (uint8_t)t->value->int32;
      g_hub_config.lp_down_type = v >> 4;
      g_hub_config.lp_down_data = v & 0x0F;
    }
    if ((t = dict_find(iterator, KEY_HUB_LP_SELECT))) {
      uint8_t v = (uint8_t)t->value->int32;
      g_hub_config.lp_select_type = v >> 4;
      g_hub_config.lp_select_data = v & 0x0F;
    }
    if ((t = dict_find(iterator, KEY_HUB_VIEWS))) {
      const char *views_str = t->value->cstring;
      g_hub_config.view_count = 0;
      for (const char *p = views_str;
           *p && g_hub_config.view_count < HUB_MAX_VIEWS; p++) {
        if (*p >= '0' && *p <= '9') {
          g_hub_config.view_order[g_hub_config.view_count++] = *p - '0';
        }
      }
    }

    // Parse custom menu/widget data
    if ((t = dict_find(iterator, KEY_HUB_MENU_UP)))
      hub_config_parse_menu(t->value->cstring, true);
    if ((t = dict_find(iterator, KEY_HUB_MENU_DOWN)))
      hub_config_parse_menu(t->value->cstring, false);
    if ((t = dict_find(iterator, KEY_HUB_WIDGETS_UP)))
      hub_config_parse_widgets(t->value->cstring, true);
    if ((t = dict_find(iterator, KEY_HUB_WIDGETS_DOWN)))
      hub_config_parse_widgets(t->value->cstring, false);
    if ((t = dict_find(iterator, KEY_HUB_ANIM)))
      g_hub_config.anim_enabled = (t->value->int32 == 1) ? 1 : 0;

    hub_config_save();
    hub_timeout_reset();
  }

  // Stock data messages from JS
  Tuple *stock_count_tuple = dict_find(iterator, KEY_STOCK_COUNT);
  if (stock_count_tuple) {
    stock_panel_count = (uint8_t)stock_count_tuple->value->int32;
    if (stock_panel_count > STOCK_MAX_PANELS)
      stock_panel_count = STOCK_MAX_PANELS;
    persist_write_int(KEY_STOCK_COUNT, stock_panel_count);
  }

  Tuple *stock_data_tuple = dict_find(iterator, KEY_STOCK_DATA);
  if (stock_data_tuple) {
    // Format: "idx|symbol|price|change|h0,h1,...,h9|price_min|price_max"
    const char *s = stock_data_tuple->value->cstring;
    int idx = 0;
    // Parse index
    while (*s >= '0' && *s <= '9') {
      idx = idx * 10 + (*s - '0');
      s++;
    }
    if (*s == '|')
      s++;
    if (idx < STOCK_MAX_PANELS) {
      StockPanel p;
      memset(&p, 0, sizeof(p));
      // Parse symbol
      int i = 0;
      while (*s && *s != '|' && i < (int)sizeof(p.symbol) - 1)
        p.symbol[i++] = *s++;
      p.symbol[i] = '\0';
      if (*s == '|')
        s++;
      // Parse price
      i = 0;
      while (*s && *s != '|' && i < (int)sizeof(p.price) - 1)
        p.price[i++] = *s++;
      p.price[i] = '\0';
      if (*s == '|')
        s++;
      // Parse change
      i = 0;
      while (*s && *s != '|' && i < (int)sizeof(p.change) - 1)
        p.change[i++] = *s++;
      p.change[i] = '\0';
      p.positive = (p.change[0] != '-');
      if (*s == '|')
        s++;
      // Parse history points (comma-separated uint8)
      for (int h = 0; h < STOCK_HISTORY_POINTS && *s; h++) {
        int val = 0;
        while (*s >= '0' && *s <= '9') {
          val = val * 10 + (*s - '0');
          s++;
        }
        if (val > 100)
          val = 100;
        p.history[h] = (uint8_t)val;
        if (*s == ',')
          s++;
      }
      // Parse price_min
      if (*s == '|')
        s++;
      i = 0;
      while (*s && *s != '|' && i < (int)sizeof(p.price_min) - 1)
        p.price_min[i++] = *s++;
      p.price_min[i] = '\0';
      // Parse price_max
      if (*s == '|')
        s++;
      i = 0;
      while (*s && *s != '|' && i < (int)sizeof(p.price_max) - 1)
        p.price_max[i++] = *s++;
      p.price_max[i] = '\0';
      // Persist this panel individually
      persist_write_data(HUB_PERSIST_STOCK0 + idx, &p, sizeof(StockPanel));
      if (idx == stock_panel_count - 1) {
        layer_mark_dirty(layer);
      }
    }
  }
}

// Forward declaration for weather retry
static void do_send_weather_request(void);

static void weather_retry_timer_callback(void *context) {
  s_weather_retry_timer = NULL;
  if (s_weather_request_pending && flags.is_connected) {
    do_send_weather_request();
  }
}

static void do_send_weather_request(void) {
  if (!s_appmsg_open)
    return;
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, 0, 0);
    result = app_message_outbox_send();
    if (result == APP_MSG_OK) {
      s_weather_request_pending = false;
      if (s_weather_retry_timer) {
        app_timer_cancel(s_weather_retry_timer);
        s_weather_retry_timer = NULL;
      }
      return;
    }
  }
  // Failed - schedule retry
  if (!s_weather_retry_timer) {
    s_weather_retry_timer = app_timer_register(
        WEATHER_RETRY_DELAY_MS, weather_retry_timer_callback, NULL);
  }
}

static void outbox_failed_callback(DictionaryIterator *iterator,
                                   AppMessageResult reason, void *context) {
  if (s_weather_request_pending && flags.is_connected &&
      !s_weather_retry_timer) {
    s_weather_retry_timer = app_timer_register(
        WEATHER_RETRY_DELAY_MS, weather_retry_timer_callback, NULL);
  }
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  if (s_weather_request_pending) {
    do_send_weather_request();
  }
}

static void init_var() {
  t = time(NULL);
  now = *(localtime(&t));

  fontsmall = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  fontsmallbold = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  fontmedium = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  if (persist_exists(KEY_RADIO_UNITS) && persist_exists(KEY_RADIO_REFRESH) &&
      persist_exists(KEY_TOGGLE_VIBRATION) && persist_exists(KEY_TOGGLE_BT)) {

    flags.is_metric = persist_read_bool(KEY_RADIO_UNITS);
    flags.is_30mn = persist_read_bool(KEY_RADIO_REFRESH);
    flags.is_bt = persist_read_bool(KEY_TOGGLE_BT);
    flags.is_vibration = persist_read_bool(KEY_TOGGLE_VIBRATION);

    int red, green, blue;
    red = persist_read_int(KEY_COLOR_RIGHT_R);
    green = persist_read_int(KEY_COLOR_RIGHT_G);
    blue = persist_read_int(KEY_COLOR_RIGHT_B);
    color_right = GColorFromRGB(red, green, blue);

    red = persist_read_int(KEY_COLOR_LEFT_R);
    green = persist_read_int(KEY_COLOR_LEFT_G);
    blue = persist_read_int(KEY_COLOR_LEFT_B);
    color_left = GColorFromRGB(red, green, blue);
  } else {
    flags.is_metric = true;
    flags.is_vibration = false;
    flags.is_bt = false;
    flags.is_30mn = true;
    color_right = GColorBlack;
    color_left = GColorBlack;
  }

  if (persist_exists(KEY_LAST_REFRESH) && persist_exists(KEY_TEMPERATURE) &&
      persist_exists(KEY_ICON)) {

    last_refresh = persist_read_int(KEY_LAST_REFRESH);

    if (FORCE_REFRESH == 1)
      last_refresh = 0;

    weather_temp = persist_read_int(KEY_TEMPERATURE);
    persist_read_string(KEY_ICON, icon, sizeof(icon));

    wind_speed_val =
        persist_exists(KEY_WIND_SPEED) ? persist_read_int(KEY_WIND_SPEED) : 0;
    humidity =
        persist_exists(KEY_HUMIDITY) ? persist_read_int(KEY_HUMIDITY) : 0;
    tmin_val = persist_exists(KEY_TMIN) ? persist_read_int(KEY_TMIN) : 0;
    tmax_val = persist_exists(KEY_TMAX) ? persist_read_int(KEY_TMAX) : 0;

    npoolTemp =
        persist_exists(KEY_POOLTEMP) ? persist_read_int(KEY_POOLTEMP) : 0;
    npoolPH = persist_exists(KEY_POOLPH) ? persist_read_int(KEY_POOLPH) : 0;
    npoolORP = persist_exists(KEY_poolORP) ? persist_read_int(KEY_poolORP) : 0;

    if (persist_exists(KEY_LOCATION))
      persist_read_string(KEY_LOCATION, location, sizeof(location));
    if (persist_exists(KEY_FORECAST_ICON1))
      persist_read_string(KEY_FORECAST_ICON1, icon1, sizeof(icon1));
    if (persist_exists(KEY_FORECAST_ICON2))
      persist_read_string(KEY_FORECAST_ICON2, icon2, sizeof(icon2));
    if (persist_exists(KEY_FORECAST_ICON3))
      persist_read_string(KEY_FORECAST_ICON3, icon3, sizeof(icon3));

    graph_hours[0] = persist_read_int(KEY_FORECAST_H0);

    // Restore compact arrays (new format)
    if (persist_exists(KEY_FORECAST_TEMP1))
      persist_read_data(KEY_FORECAST_TEMP1, graph_temps, sizeof(graph_temps));
    if (persist_exists(KEY_FORECAST_RAIN1))
      persist_read_data(KEY_FORECAST_RAIN1, graph_rains, sizeof(graph_rains));
    if (persist_exists(KEY_FORECAST_WIND1))
      persist_read_data(KEY_FORECAST_WIND1, graph_wind_val,
                        sizeof(graph_wind_val));
    if (persist_exists(KEY_FORECAST_H1))
      persist_read_data(KEY_FORECAST_H1, graph_hours, sizeof(graph_hours));

    // 3-day forecast
    if (persist_exists(KEY_DAY1_TEMP))
      persist_read_data(KEY_DAY1_TEMP, days_temp, sizeof(days_temp));
    if (persist_exists(KEY_DAY1_ICON))
      persist_read_data(KEY_DAY1_ICON, days_icon, sizeof(days_icon));
    if (persist_exists(KEY_DAY1_RAIN))
      persist_read_data(KEY_DAY1_RAIN, days_rain, sizeof(days_rain));
    if (persist_exists(KEY_DAY1_WIND))
      persist_read_data(KEY_DAY1_WIND, days_wind, sizeof(days_wind));

  } else {
    last_refresh = 0;
    wind_speed_val = 0;
    tmin_val = 0;
    humidity = 0;
    tmax_val = 0;
    weather_temp = 0;

    snprintf(icon, sizeof(icon), " ");
    snprintf(icon1, sizeof(icon1), " ");
    snprintf(icon2, sizeof(icon2), " ");
    snprintf(icon3, sizeof(icon3), " ");
    snprintf(location, sizeof(location), " ");

    memset(graph_temps, 10, sizeof(graph_temps));
    memset(graph_rains, 0, sizeof(graph_rains));
    memset(graph_wind_val, 0, sizeof(graph_wind_val));
    graph_hours[0] = 0;
    graph_hours[1] = 3;
    graph_hours[2] = 6;
    graph_hours[3] = 9;

    snprintf(days_icon[0], sizeof(days_icon[0]), " ");
    snprintf(days_icon[1], sizeof(days_icon[1]), " ");
    snprintf(days_icon[2], sizeof(days_icon[2]), " ");
  }

  // Restore stock panel count (panels loaded on demand from persist)
  if (persist_exists(KEY_STOCK_COUNT)) {
    stock_panel_count = persist_read_int(KEY_STOCK_COUNT);
    if (stock_panel_count > STOCK_MAX_PANELS)
      stock_panel_count = STOCK_MAX_PANELS;
  }

  color_temp = GColorWhite;
  color_left = GColorBlack;
  color_right = GColorBlack;

  assign_fonts();

  BatteryChargeState charge_state = battery_state_service_peek();
  flags.is_charging = charge_state.is_charging;
  flags.is_connected = connection_service_peek_pebble_app_connection();

  if (flags.is_30mn)
    duration = 1800;
  else
    duration = 3600;
}

// --- Hub timeout callback: return to watchface ---
static void hub_timeout_fired(void) {
  while (window_stack_get_top_window() != s_main_window) {
    window_stack_pop(false);
  }
  current_view_index = 0;
  layer_mark_dirty(layer);
}

// --- Button handlers ---
static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Reset to default view; don't exit app on back press
  if (current_view_index != 0) {
    current_view_index = 0;
    layer_mark_dirty(layer);
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  hub_timeout_reset();
  if (g_hub_config.btn_up_type == HUB_OBJ_MENU) {
    hub_menu_push(true, HUB_DIR_UP);
  } else {
    hub_widgets_push(true, HUB_DIR_UP);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  hub_timeout_reset();
  if (g_hub_config.btn_down_type == HUB_OBJ_MENU) {
    hub_menu_push(false, HUB_DIR_DOWN);
  } else {
    hub_widgets_push(false, HUB_DIR_DOWN);
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Cycle watchface views
  if (g_hub_config.view_count > 1) {
    current_view_index = (current_view_index + 1) % g_hub_config.view_count;
    layer_mark_dirty(layer);
  }
}

static void execute_long_press(uint8_t type, uint8_t data) {
  vibes_double_pulse();
  if (type == HUB_LP_PSEUDOAPP) {
    hub_pseudoapp_push(data);
  } else if (type == HUB_LP_WEBHOOK) {
    hub_action_execute(data);
  }
}

static void up_long_handler(ClickRecognizerRef recognizer, void *context) {
  hub_timeout_reset();
  execute_long_press(g_hub_config.lp_up_type, g_hub_config.lp_up_data);
}

static void down_long_handler(ClickRecognizerRef recognizer, void *context) {
  hub_timeout_reset();
  execute_long_press(g_hub_config.lp_down_type, g_hub_config.lp_down_data);
}

static void select_long_handler(ClickRecognizerRef recognizer, void *context) {
  hub_timeout_reset();
  execute_long_press(g_hub_config.lp_select_type, g_hub_config.lp_select_data);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 400, up_long_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 400, down_long_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 400, select_long_handler, NULL);
}

static void init() {
  // Open AppMessage first, before any heap allocations, to maximise the chance
  // of succeeding even when 824B of AppMessage pool is orphaned from a prior
  // crash.
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  AppMessageResult msg_result = app_message_open(512, 32);
  s_appmsg_open = (msg_result == APP_MSG_OK);
  APP_LOG(APP_LOG_LEVEL_INFO, "app_message_open: %d heap=%zu", (int)msg_result,
          heap_bytes_free());

  init_var();
  hub_config_init();
  APP_LOG(APP_LOG_LEVEL_INFO, "post-cfg heap=%zu", heap_bytes_free());
  s_main_window = window_create();
  window_stack_push(s_main_window, true);
  window_set_click_config_provider(s_main_window, click_config_provider);

  s_canvas_layer = window_get_root_layer(s_main_window);
  layer = layer_create(layer_get_bounds(s_canvas_layer));
  layer_set_update_proc(layer, update_proc);
  layer_add_child(s_canvas_layer, layer);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  bluetooth_connection_service_subscribe(bt_handler);

  app_focus_service_subscribe_handlers((AppFocusHandlers){
      .did_focus = app_focus_changed, .will_focus = app_focus_changing});
  hub_set_main_window(s_main_window);
  hub_timeout_init(hub_timeout_fired);

  // Mark init complete — callbacks (focus, tick) may now safely send
  // AppMessages.
  s_init_done = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "HEAP post-init: %zu", heap_bytes_free());
  // Trigger a redraw now that init is done (the initial window_stack_push draw
  // was skipped because s_init_done was false at that point).
  layer_mark_dirty(layer);
}

static void deinit() {

  hub_timeout_deinit();

  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();

  if (s_weather_retry_timer) {
    app_timer_cancel(s_weather_retry_timer);
    s_weather_retry_timer = NULL;
  }

  app_message_deregister_callbacks();

  layer_destroy(layer);
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
