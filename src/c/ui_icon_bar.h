#pragma once

#include <pebble.h>

typedef struct {
  GFont fontsmall;
  GFont fontsmallbold;
  GFont fontmedium;
  GColor color_temp;
  const char *week_day;
  const char *mday;
  const char *min_temp_text;
  const char *max_temp_text;
  const char *weather_temp_text;
  bool has_fresh_weather;
  bool is_connected;
  bool is_quiet_time;
  int humidity;
  int wind_speed_val;
  int met_unit;
  int icon_id;
} IconBarData;

void ui_draw_icon_bar(GContext *ctx, const IconBarData *data);
