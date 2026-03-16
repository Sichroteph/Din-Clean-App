#pragma once

#include <pebble.h>

typedef struct {
  uint16_t icon_id;
  uint8_t humidity;
  uint8_t wind_speed_val;
  uint8_t met_unit;
  bool has_fresh_weather;
  bool is_connected;
  bool is_quiet_time;
} IconBarData;

void ui_draw_icon_bar(GContext *ctx, const IconBarData *data);
