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
Layer *g_main_layer;

static char week_day[4] = " ";
static char mday[3] = " ";
static char weather_temp_char[7] = " ";
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

static char icon[3] = " ";

// Hourly forecast data (9 temps for 0-24h, 12 rain bars, 8 winds for 0-24h, 4
// hours for 0-12h)
int8_t graph_temps[9] = {10, 10, 10, 10, 10, 10, 10, 10, 10};
uint8_t graph_rains[12] = {0};
uint8_t graph_wind_val[8] = {0};
uint8_t graph_hours[4] = {0, 3, 6, 9};

// 3-day forecast data (integer storage — formatted on draw)
int8_t days_temp_v[5] = {-128, -128, -128, -128, -128}; // -128 = no data
char days_icon[5][3] = {"", "", "", "", ""};
uint8_t days_rain_v[5] = {0};
uint8_t days_wind_v[5] = {0};
char wind_unit_str[5] = "km/h";

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

static void app_focus_changed(bool focused) {
  if (focused) {
    layer_set_hidden(layer, false);
    layer_mark_dirty(layer);

    if (!s_init_done || !s_appmsg_open)
      return;

    // Check if weather data is stale and request refresh
    t = time(NULL);
    now = *(localtime(&t));
    bool weather_stale =
        flags.is_connected && ((mktime(&now) - last_refresh) > duration);

    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
      dict_write_uint8(iter, 0, 0);
      if (weather_stale)
        s_weather_request_pending = true;
      if (app_message_outbox_send() == APP_MSG_OK && weather_stale) {
        s_weather_request_pending = false;
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
  return weather_utils_build_icon(text_icon);
}

// Compact centered text helper
static void dtext(GContext *c, const char *s, GFont f, int y, int h) {
  graphics_draw_text(c, s, f, GRect(0, y, 144, h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

// Alternative view renderer (0 heap alloc — draws on existing GContext)
static void draw_alt_view(GContext *ctx, uint8_t vid, int icon_id, bool fresh) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  GFont fb = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont fs = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  char buf[24];

  if (vid == HUB_VIEW_WEATHER) {
    // Icon centré en haut
    GBitmap *bmp = gbitmap_create_with_resource(icon_id);
    if (bmp) {
      graphics_draw_bitmap_in_rect(ctx, bmp, GRect(54, 2, 35, 35));
      gbitmap_destroy(bmp);
    }
    // Température actuelle
    dtext(ctx, weather_temp_char, fb, 38, 34);
    // Min / Max (même police que la temp)
    snprintf(buf, sizeof(buf), "%s / %s", minTemp, maxTemp);
    dtext(ctx, buf, fb, 76, 34);
    // Vent + Humidité inversés (même police)
    snprintf(buf, sizeof(buf), "%d%s  %d%%", wind_speed_val, wind_unit_str,
             humidity);
    dtext(ctx, buf, fb, 114, 34);
    // Heure de mise à jour (petite)
    if (fresh) {
      struct tm *ref = localtime(&last_refresh);
      snprintf(buf, sizeof(buf), "Maj %02d:%02d", ref->tm_hour, ref->tm_min);
    } else {
      snprintf(buf, sizeof(buf), "Offline");
    }
    dtext(ctx, buf, fs, 152, 16);
  } else if (vid == HUB_VIEW_ANALOG) {
// Analog clock — thick lines with rounded ends + 12 hour markers
#define AC_CX 72
#define AC_CY 84
#define AC_R 60
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx, GColorWhite);
    // 12 hour markers
    for (int i = 0; i < 12; i++) {
      int32_t a = TRIG_MAX_ANGLE * i / 12;
      int mx = AC_CX + sin_lookup(a) * (AC_R - 4) / TRIG_MAX_RATIO;
      int my = AC_CY - cos_lookup(a) * (AC_R - 4) / TRIG_MAX_RATIO;
      graphics_fill_circle(ctx, GPoint(mx, my), (i % 3 == 0) ? 3 : 1);
    }
    // Hour hand
    int h12 = now.tm_hour % 12;
    int32_t ha = TRIG_MAX_ANGLE * (h12 * 60 + now.tm_min) / 720;
    GPoint hend = GPoint(AC_CX + sin_lookup(ha) * 36 / TRIG_MAX_RATIO,
                         AC_CY - cos_lookup(ha) * 36 / TRIG_MAX_RATIO);
    graphics_context_set_stroke_width(ctx, 7);
    graphics_draw_line(ctx, GPoint(AC_CX, AC_CY), hend);
    // Minute hand
    int32_t ma = TRIG_MAX_ANGLE * now.tm_min / 60;
    GPoint mend = GPoint(AC_CX + sin_lookup(ma) * 52 / TRIG_MAX_RATIO,
                         AC_CY - cos_lookup(ma) * 52 / TRIG_MAX_RATIO);
    graphics_context_set_stroke_width(ctx, 5);
    graphics_draw_line(ctx, GPoint(AC_CX, AC_CY), mend);
    // Center dot
    graphics_fill_circle(ctx, GPoint(AC_CX, AC_CY), 3);
#undef AC_CX
#undef AC_CY
#undef AC_R
  } else {
    BatteryChargeState bat = battery_state_service_peek();
    // --- "Battery" label ---
    dtext(ctx, "Battery", fs, 8, 16);
    // --- Gauge body : moitié de largeur, plus épaisse, centrée ---
    // Gauge: 62px wide x 26px tall, centered (x=38), nub 7x16 on right
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(38, 26, 62, 26), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorBlack);
    int c_px = bat.charge_percent * 60 / 100;
    graphics_fill_rect(ctx, GRect(39 + c_px, 27, 60 - c_px, 24), 0,
                       GCornerNone);
    // --- Borne positive (nub) à droite ---
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(100, 31, 7, 16), 0, GCornerNone);
    // --- Countdown ---
    if (persist_exists(HUB_PERSIST_COUNTDOWN)) {
      CountdownData cd;
      memset(&cd, 0, sizeof(CountdownData));
      persist_read_data(HUB_PERSIST_COUNTDOWN, &cd, sizeof(CountdownData));
      int days = (int)((cd.ts - time(NULL)) / 86400);
      if (days > 0) {
        bool has_label = cd.label[0] != '\0';
        if (has_label) {
          dtext(ctx, cd.label, fb, 80, 34);
        }
        snprintf(buf, sizeof(buf), "J-%d", days);
        dtext(ctx, buf, fb, has_label ? 118 : 96, 34);
      }
    }
  }
}

// --- Action toast overlay (zero heap in BSS; only the AppTimer entry is heap)
// ---

static AppTimer *s_action_toast_timer = NULL;

// --- Exit hint (double-back to exit) ---
static uint8_t s_back_count = 0;
static AppTimer *s_back_timer = NULL;
static bool s_show_exit_hint = false;

static void action_toast_clear_cb(void *context) {
  s_action_toast_timer = NULL;
  g_hub_action_toast = false;
  layer_mark_dirty(layer);
}

// Shared toast overlay: black rounded rect + white text (zero heap).
static void draw_toast(GContext *ctx, const char *text) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(10, 64, 124, 36), 4, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_round_rect(ctx, GRect(10, 64, 124, 36), 4);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(12, 68, 120, 28), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

// Draws the "Action sent!" overlay over whatever is currently on screen.
// Registers a 1.5-second timer the first time it is called so the overlay
// disappears automatically. Safe to call from update_proc.
static void draw_action_toast(GContext *ctx) {
  if (!g_hub_action_toast)
    return;
  if (!s_action_toast_timer)
    s_action_toast_timer =
        app_timer_register(1500, action_toast_clear_cb, NULL);
  draw_toast(ctx, "Action sent!");
}

static void update_proc(Layer *layer, GContext *ctx) {
  // Static local for IconBarData to avoid stack overflow on real hardware
  static IconBarData icon_data;

  if (!s_init_done) {
    return;
  }

  if (window_stack_get_top_window() != s_main_window) {
    return;
  }

  int icon_id;
  icon_id = build_icon_with_pool_check(icon);

  // Draw background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, 160, 180), 0, GCornerNone);

  t = time(NULL);
  now = *(localtime(&t));

  const char *locale = i18n_get_system_locale();
  snprintf(week_day, sizeof(week_day), "%s",
           weather_utils_get_weekday_abbrev(locale, now.tm_wday));

  snprintf(mday, sizeof(mday), "%i", now.tm_mday);
  graphics_context_set_text_color(ctx, GColorWhite);

  bool has_fresh_weather =
      ((mktime(&now) - last_refresh) < duration + offline_delay);

  snprintf(weather_temp_char, sizeof(weather_temp_char), "%i\xc2\xb0", weather_temp);
  snprintf(minTemp, sizeof(minTemp), "%i\xc2\xb0", tmin_val);
  snprintf(maxTemp, sizeof(maxTemp), "%i\xc2\xb0", tmax_val);

  // Alternative views: configurable via view_order
  if (current_view_index > 0 && current_view_index < g_hub_config.view_count) {
    uint8_t vid = g_hub_config.view_order[current_view_index];
    if (vid == HUB_VIEW_WEATHER || vid == HUB_VIEW_DATE ||
        vid == HUB_VIEW_ANALOG) {
      draw_alt_view(ctx, vid, icon_id, has_fresh_weather);
      draw_action_toast(ctx);
      if (g_hub_ring_active)
        draw_toast(ctx, g_hub_ring_active == 1 ? "Timer!" : "Alarm!");
      return;
    }
  }

  // Draw hours FIRST for instant display (only 4 small bitmaps)
  char heure[5];
  if (clock_is_24h_style()) {
    strftime(heure, sizeof(heure), "%H%M", &now);
  } else {
    strftime(heure, sizeof(heure), "%I%M", &now);
  }
  ui_draw_time(ctx, heure);

  // Draw icon bar AFTER time (loads many bitmaps, slower)
  icon_data.week_day = week_day;
  icon_data.mday = mday;
  icon_data.min_temp_text = minTemp;
  icon_data.max_temp_text = maxTemp;
  icon_data.weather_temp_text = weather_temp_char;
  icon_data.has_fresh_weather = has_fresh_weather;
  icon_data.is_connected = flags.is_connected;
  icon_data.is_quiet_time = quiet_time_is_active();
  icon_data.humidity = humidity;
  icon_data.wind_speed_val = wind_speed_val;
  icon_data.met_unit = flags.is_metric ? 20 : 25;
  icon_data.icon_id = icon_id;
  ui_draw_icon_bar(ctx, &icon_data);

  draw_action_toast(ctx);

  // Ring toast (timer/alarm expiry)
  if (g_hub_ring_active)
    draw_toast(ctx, g_hub_ring_active == 1 ? "Timer!" : "Alarm!");

  // Exit hint toast (double-back to exit)
  if (s_show_exit_hint)
    draw_toast(ctx, "Press < to exit");
}

// Forward declaration (used in handle_tick, inbox handler, and retry callback)
static void do_send_weather_request(void);

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
      s_weather_request_pending = true;
      do_send_weather_request();
    }
  }

  layer_mark_dirty(layer);
}

void bt_handler(bool connected) {
  flags.is_connected = connected;
  if (flags.is_bt) {
    vibes_double_pulse();
  }
  layer_mark_dirty(layer);
}

static void hub_timeout_fired(void);

// Helper: copy chars from s until '|' or end, into dst (max n-1 chars + NUL).
static const char *cpf(const char *s, char *dst, int n) {
  int i = 0;
  while (*s && *s != '|' && i < n - 1) dst[i++] = *s++;
  dst[i] = '\0';
  if (*s == '|') s++;
  return s;
}

static void inbox_received_callback(DictionaryIterator *iterator,
                                    void *context) {
  // Read tuples for data
  Tuple *radio_tuple = dict_find(iterator, KEY_RADIO_UNITS);
  Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);

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
    // 5-day forecast: days 1-3 base=200+d*4, days 4-5 base=204+d*4
    for (int d = 0; d < 5; d++) {
      uint32_t base = (d < 3 ? 200 : 204) + d * 4;
      if ((t = dict_find(iterator, base)))
        days_temp_v[d] = (int8_t)atoi(t->value->cstring);
      if ((t = dict_find(iterator, base + 1)))
        snprintf(days_icon[d], sizeof(days_icon[d]), "%s", t->value->cstring);
      if ((t = dict_find(iterator, base + 2)))
        days_rain_v[d] = (uint8_t)atoi(t->value->cstring);
      if ((t = dict_find(iterator, base + 3))) {
        days_wind_v[d] = (uint8_t)atoi(t->value->cstring);
        if (d == 0) {
          const char *p = t->value->cstring;
          while (*p >= '0' && *p <= '9') p++;
          const char *u = "km/h";
          if (*p == 'm') u = (p[1] == '/') ? "m/s" : "mph";
          memcpy(wind_unit_str, u, 5);
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

    persist_write_data(KEY_DAY1_TEMP, days_temp_v, sizeof(days_temp_v));
    persist_write_data(KEY_DAY1_ICON, days_icon, sizeof(days_icon));
    persist_write_data(KEY_DAY1_RAIN, days_rain_v, sizeof(days_rain_v));
    persist_write_data(KEY_DAY1_WIND, days_wind_v, sizeof(days_wind_v));

    layer_mark_dirty(layer);
  }

  // Configuration message received
  if (radio_tuple) {
    Tuple *refresh_tuple = dict_find(iterator, KEY_RADIO_REFRESH);
    Tuple *vibration_tuple = dict_find(iterator, KEY_TOGGLE_VIBRATION);
    Tuple *bt_tuple = dict_find(iterator, KEY_TOGGLE_BT);

    flags.is_bt = bt_tuple ? (bt_tuple->value->int32 == 1) : flags.is_bt;
    flags.is_metric =
        radio_tuple ? (radio_tuple->value->int32 != 1) : flags.is_metric;
    flags.is_30mn =
        refresh_tuple ? (refresh_tuple->value->int32 == 1) : flags.is_30mn;
    flags.is_vibration = vibration_tuple ? (vibration_tuple->value->int32 == 1)
                                         : flags.is_vibration;

    persist_write_bool(KEY_RADIO_UNITS, flags.is_metric);
    persist_write_bool(KEY_RADIO_REFRESH, flags.is_30mn);
    persist_write_bool(KEY_TOGGLE_BT, flags.is_bt);
    persist_write_bool(KEY_TOGGLE_VIBRATION, flags.is_vibration);

    light_enable_interaction();
    vibes_double_pulse();

    // Return to main clock view and refresh
    hub_timeout_fired();

    // Request immediate weather update to apply new units
    if (s_appmsg_open) {
      s_weather_request_pending = true;
      do_send_weather_request();
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

    if ((t = dict_find(iterator, KEY_HUB_COUNTDOWN))) {
      int32_t ts = t->value->int32;
      if (ts == -1) {
        persist_delete(HUB_PERSIST_COUNTDOWN);
      } else {
        CountdownData cd;
        memset(&cd, 0, sizeof(CountdownData));
        if (persist_exists(HUB_PERSIST_COUNTDOWN) &&
            persist_get_size(HUB_PERSIST_COUNTDOWN) ==
                (int)sizeof(CountdownData)) {
          persist_read_data(HUB_PERSIST_COUNTDOWN, &cd, sizeof(CountdownData));
        }
        cd.ts = ts;
        Tuple *lt = dict_find(iterator, KEY_HUB_COUNTDOWN_LABEL);
        if (lt) {
          strncpy(cd.label, lt->value->cstring, sizeof(cd.label) - 1);
          cd.label[sizeof(cd.label) - 1] = '\0';
        }
        persist_write_data(HUB_PERSIST_COUNTDOWN, &cd, sizeof(CountdownData));
      }
    }

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
      s = cpf(s, p.symbol, sizeof(p.symbol));
      s = cpf(s, p.price, sizeof(p.price));
      s = cpf(s, p.change, sizeof(p.change));
      p.positive = (p.change[0] != '-');
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
      // Parse price_min/max
      if (*s == '|') s++;
      s = cpf(s, p.price_min, sizeof(p.price_min));
      s = cpf(s, p.price_max, sizeof(p.price_max));
      // Persist this panel individually
      persist_write_data(HUB_PERSIST_STOCK0 + idx, &p, sizeof(StockPanel));
      if (idx == stock_panel_count - 1) {
        layer_mark_dirty(layer);
      }
    }
  }
}

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
    dict_write_uint8(iter, KEY_HEAP_FREE, (uint8_t)(heap_bytes_free() >> 7));
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

  if (persist_exists(KEY_RADIO_UNITS) && persist_exists(KEY_RADIO_REFRESH) &&
      persist_exists(KEY_TOGGLE_VIBRATION) && persist_exists(KEY_TOGGLE_BT)) {

    flags.is_metric = persist_read_bool(KEY_RADIO_UNITS);
    flags.is_30mn = persist_read_bool(KEY_RADIO_REFRESH);
    flags.is_bt = persist_read_bool(KEY_TOGGLE_BT);
    flags.is_vibration = persist_read_bool(KEY_TOGGLE_VIBRATION);
  } else {
    flags.is_metric = true;
    flags.is_vibration = false;
    flags.is_bt = false;
    flags.is_30mn = true;
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

    // 3-day forecast (integer storage)
    if (persist_exists(KEY_DAY1_TEMP) &&
        persist_get_size(KEY_DAY1_TEMP) == (int)sizeof(days_temp_v))
      persist_read_data(KEY_DAY1_TEMP, days_temp_v, sizeof(days_temp_v));
    if (persist_exists(KEY_DAY1_ICON) &&
        persist_get_size(KEY_DAY1_ICON) == (int)sizeof(days_icon))
      persist_read_data(KEY_DAY1_ICON, days_icon, sizeof(days_icon));
    if (persist_exists(KEY_DAY1_RAIN) &&
        persist_get_size(KEY_DAY1_RAIN) == (int)sizeof(days_rain_v))
      persist_read_data(KEY_DAY1_RAIN, days_rain_v, sizeof(days_rain_v));
    if (persist_exists(KEY_DAY1_WIND) &&
        persist_get_size(KEY_DAY1_WIND) == (int)sizeof(days_wind_v))
      persist_read_data(KEY_DAY1_WIND, days_wind_v, sizeof(days_wind_v));

  } else {
    last_refresh = 0;
    wind_speed_val = 0;
    tmin_val = 0;
    humidity = 0;
    tmax_val = 0;
    weather_temp = 0;

    snprintf(icon, sizeof(icon), " ");

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
static void back_exit_hint_clear(void *context) {
  s_back_count = 0;
  s_show_exit_hint = false;
  s_back_timer = NULL;
  layer_mark_dirty(layer);
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (g_hub_ring_active) {
    hub_ring_dismiss();
    layer_mark_dirty(layer);
    return;
  }
  // If not on default view, switch back first and reset counter
  if (current_view_index != 0) {
    current_view_index = 0;
    s_back_count = 0;
    s_show_exit_hint = false;
    if (s_back_timer) {
      app_timer_cancel(s_back_timer);
      s_back_timer = NULL;
    }
    layer_mark_dirty(layer);
    return;
  }

  if (s_back_timer) {
    app_timer_cancel(s_back_timer);
    s_back_timer = NULL;
  }

  s_back_count++;

  if (s_back_count >= 3) {
    // Third press: exit app
    window_stack_pop_all(false);
    return;
  }

  if (s_back_count == 2) {
    // Second press: show exit hint toast
    s_show_exit_hint = true;
    vibes_short_pulse();
    layer_mark_dirty(layer);
  }
  // First press: screen wakes up, counter starts silently

  // Auto-clear after 2 seconds
  s_back_timer = app_timer_register(2000, back_exit_hint_clear, NULL);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (g_hub_ring_active) {
    hub_ring_dismiss();
    layer_mark_dirty(layer);
    return;
  }
  hub_timeout_reset();
  if (g_hub_config.btn_up_type == HUB_OBJ_MENU) {
    hub_menu_push(true, HUB_DIR_UP);
  } else {
    hub_widgets_push(true, HUB_DIR_UP);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (g_hub_ring_active) {
    hub_ring_dismiss();
    layer_mark_dirty(layer);
    return;
  }
  hub_timeout_reset();
  if (g_hub_config.btn_down_type == HUB_OBJ_MENU) {
    hub_menu_push(false, HUB_DIR_DOWN);
  } else {
    hub_widgets_push(false, HUB_DIR_DOWN);
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (g_hub_ring_active) {
    hub_ring_dismiss();
    layer_mark_dirty(layer);
    return;
  }
  if (g_hub_config.view_count > 1) {
    current_view_index = (current_view_index + 1) % g_hub_config.view_count;
    hub_timeout_reset();
    layer_mark_dirty(layer);
  }
}

static void execute_long_press(uint8_t type, uint8_t data) {
  vibes_double_pulse();
  if (type == HUB_LP_PSEUDOAPP) {
    hub_pseudoapp_push(data);
  } else if (type == HUB_LP_WEBHOOK) {
    hub_action_execute(data);
    // Force an immediate redraw so the toast overlay appears right away.
    // (app_focus_changed won't fire since we stay on the main window.)
    layer_mark_dirty(layer);
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

  // Mark init complete — callbacks (focus, tick) may now safely send
  // AppMessages.
  s_init_done = true;
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

  if (s_action_toast_timer) {
    app_timer_cancel(s_action_toast_timer);
    s_action_toast_timer = NULL;
  }

  app_message_deregister_callbacks();

  g_main_layer = NULL;
  layer_destroy(layer);
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
