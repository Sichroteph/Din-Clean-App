#include <pebble.h>

#include "ui_icon_bar.h"

// String globals from Din_Clean.c (extern access avoids pointer fields in IconBarData)
extern char week_day[];
extern char mday[];
extern char weather_temp_char[];
extern char minTemp[];
extern char maxTemp[];

// Layout constants (previously computed in Din_Clean.c update_proc)
#define IB_DAYW_X 0
#define IB_DAYW_Y 0
#define IB_DAY_X 0
#define IB_DAY_Y 6
#define IB_SIDEBAR 36
#define IB_TEMP_X (-11)
#define IB_TEMP_Y 75
#define IB_TMIN_X (-5)
#define IB_TMIN_Y 131
#define IB_TMAX_X (-5)
#define IB_TMAX_Y 144
#define IB_ICON_X 0
#define IB_ICON_Y 39
#define IB_BT_X (-1)
#define IB_BT_Y 16

static void draw_humidity_icons(GContext *ctx, const IconBarData *d) {
  if (d->humidity <= 0) {
    return;
  }

  const GRect hum1 = {{5, 116}, {7, 10}};
  const GRect hum2 = {{14, 116}, {7, 10}};
  const GRect hum3 = {{23, 116}, {7, 10}};

  if (d->humidity > 40 && d->humidity < 60) {
    GBitmap *leaf = gbitmap_create_with_resource(RESOURCE_ID_LEAF);
    if (leaf) {
      graphics_draw_bitmap_in_rect(ctx, leaf, (GRect){{12, 116}, {11, 10}});
      gbitmap_destroy(leaf);
    }
    return;
  }

  // High humidity: load HUMIDITY icon once, draw multiple times
  if (d->humidity >= 60) {
    GBitmap *hum = gbitmap_create_with_resource(RESOURCE_ID_HUMIDITY);
    if (hum) {
      graphics_draw_bitmap_in_rect(ctx, hum, hum1);
      if (d->humidity >= 70)
        graphics_draw_bitmap_in_rect(ctx, hum, hum2);
      if (d->humidity >= 80)
        graphics_draw_bitmap_in_rect(ctx, hum, hum3);
      gbitmap_destroy(hum);
    }
    return;
  }

  // Low humidity: load DRY icon once, draw multiple times
  GBitmap *dry = gbitmap_create_with_resource(RESOURCE_ID_DRY);
  if (dry) {
    graphics_draw_bitmap_in_rect(ctx, dry, hum1);
    if (d->humidity <= 30)
      graphics_draw_bitmap_in_rect(ctx, dry, hum2);
    if (d->humidity <= 20)
      graphics_draw_bitmap_in_rect(ctx, dry, hum3);
    gbitmap_destroy(dry);
  }
}

// Load each wind overlay POURTOURW1-4 only if wind exceeds threshold
static void draw_wind_overlays(GContext *ctx, int wind_val, int met_unit) {
  static const uint32_t s_wind_ids[] = {
    RESOURCE_ID_POURTOURW1, RESOURCE_ID_POURTOURW2,
    RESOURCE_ID_POURTOURW3, RESOURCE_ID_POURTOURW4
  };
  const GRect wr = {{IB_ICON_X, IB_ICON_Y}, {35, 35}};
  for (int i = 0; i < 4; i++) {
    if (wind_val <= met_unit * (i + 1)) break;
    GBitmap *bmp = gbitmap_create_with_resource(s_wind_ids[i]);
    if (bmp) {
      graphics_draw_bitmap_in_rect(ctx, bmp, wr);
      gbitmap_destroy(bmp);
    }
  }
}

void ui_draw_icon_bar(GContext *ctx, const IconBarData *d) {
  if (!d) {
    return;
  }

  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_context_set_text_color(ctx, GColorWhite);

  if (!d->has_fresh_weather) {
    return;
  }

  GFont fsmall = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  GFont fmed = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont fbold = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // Draw background bitmap (34×168) at sidebar origin
  GBitmap *background = gbitmap_create_with_resource(RESOURCE_ID_BACKGROUND);
  if (background) {
    graphics_draw_bitmap_in_rect(ctx, background, GRect(0, 0, 34, 168));
    gbitmap_destroy(background);
  }

  const GRect dayw_r = {{IB_DAYW_X, IB_DAYW_Y}, {IB_SIDEBAR, 150}};
  const GRect day_r = {{IB_DAY_X, IB_DAY_Y}, {IB_SIDEBAR, 150}};
  const GRect bt_r = {{IB_BT_X, IB_BT_Y}, {35, 17}};

  // Connection / quiet time status
  if (!d->is_quiet_time) {
    graphics_draw_text(ctx, week_day, fsmall, dayw_r,
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    if (d->is_connected) {
      graphics_draw_text(ctx, mday, fmed, day_r,
                         GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    } else {
      GBitmap *bt = gbitmap_create_with_resource(RESOURCE_ID_BT_DISCONECT);
      if (bt) {
        graphics_draw_bitmap_in_rect(ctx, bt, bt_r);
        gbitmap_destroy(bt);
      }
    }
  } else {
    graphics_draw_text(ctx, week_day, fsmall, dayw_r,
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    GBitmap *silent = gbitmap_create_with_resource(RESOURCE_ID_SILENT);
    if (silent) {
      graphics_draw_bitmap_in_rect(ctx, silent, bt_r);
      gbitmap_destroy(silent);
    }
  }

  draw_humidity_icons(ctx, d);

  // Validate icon_id before loading to prevent crashes from invalid resource
  // IDs
  const GRect icon_r = {{IB_ICON_X, IB_ICON_Y}, {35, 35}};
  if (d->icon_id > 0 && d->icon_id < 500) {
    GBitmap *icon = gbitmap_create_with_resource(d->icon_id);
    if (icon) {
      graphics_draw_bitmap_in_rect(ctx, icon, icon_r);
      gbitmap_destroy(icon);
    }
  }

  draw_wind_overlays(ctx, d->wind_speed_val, d->met_unit);

  graphics_draw_text(ctx, weather_temp_char, fmed,
                     GRect(IB_TEMP_X, IB_TEMP_Y, 60, 60),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, minTemp, fbold,
                     GRect(IB_TMIN_X, IB_TMIN_Y, 45, 35),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, maxTemp, fbold,
                     GRect(IB_TMAX_X, IB_TMAX_Y, 45, 35),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}
