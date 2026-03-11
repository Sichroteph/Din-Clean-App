#include <pebble.h>

#include "ui_time.h"

void ui_draw_time(GContext *ctx, const char *digits) {
  if (!digits || digits[3] == '\0')
    return;
  // Fixed digit positions: (60,1), (101,1), (60,86), (101,86) — all 46×81
  static const int16_t pos[][2] = {{60,1},{101,1},{60,86},{101,86}};
  static const int res[] = {
    RESOURCE_ID_0, RESOURCE_ID_1, RESOURCE_ID_2, RESOURCE_ID_3, RESOURCE_ID_4,
    RESOURCE_ID_5, RESOURCE_ID_6, RESOURCE_ID_7, RESOURCE_ID_8, RESOURCE_ID_9
  };
  for (int i = 0; i < 4; i++) {
    uint8_t d = digits[i] - '0';
    if (d > 9) d = 0;
    GBitmap *bmp = gbitmap_create_with_resource(res[d]);
    if (bmp) {
      graphics_draw_bitmap_in_rect(ctx, bmp, GRect(pos[i][0], pos[i][1], 46, 81));
      gbitmap_destroy(bmp);
    }
  }
}
