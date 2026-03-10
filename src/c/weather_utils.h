#pragma once

#include <pebble.h>

int weather_utils_build_icon(const char *code);
const char *weather_utils_get_weekday_abbrev(const char *locale, int day_index);
