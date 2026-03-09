#include "hub_pseudoapp.h"

static const char *s_app_names[] = {
  [HUB_APP_STOPWATCH] = "Stopwatch",
  [HUB_APP_TIMER]     = "Timer",
  [HUB_APP_ALARM]     = "Alarm",
};

typedef struct {
  Window *window;
  TextLayer *name_layer;
  TextLayer *stub_layer;
  uint8_t app_id;
} PseudoAppCtx;

static void pa_window_load(Window *window) {
  PseudoAppCtx *ctx = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  ctx->name_layer = text_layer_create(GRect(0, 50, bounds.size.w, 36));
  text_layer_set_text(ctx->name_layer, s_app_names[ctx->app_id]);
  text_layer_set_font(ctx->name_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(ctx->name_layer, GTextAlignmentCenter);
  text_layer_set_background_color(ctx->name_layer, GColorBlack);
  text_layer_set_text_color(ctx->name_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(ctx->name_layer));

  ctx->stub_layer = text_layer_create(GRect(0, 90, bounds.size.w, 24));
  text_layer_set_text(ctx->stub_layer, "(stub)");
  text_layer_set_font(ctx->stub_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(ctx->stub_layer, GTextAlignmentCenter);
  text_layer_set_background_color(ctx->stub_layer, GColorBlack);
  text_layer_set_text_color(ctx->stub_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(ctx->stub_layer));

  hub_timeout_reset();
}

static void pa_window_unload(Window *window) {
  PseudoAppCtx *ctx = window_get_user_data(window);
  text_layer_destroy(ctx->name_layer);
  text_layer_destroy(ctx->stub_layer);
  window_destroy(ctx->window);
  free(ctx);
}

void hub_pseudoapp_push(uint8_t app_id) {
  if (app_id >= HUB_APP_COUNT) return;

  PseudoAppCtx *ctx = malloc(sizeof(PseudoAppCtx));
  if (!ctx) return;

  ctx->app_id = app_id;
  ctx->window = window_create();
  if (!ctx->window) {
    free(ctx);
    return;
  }
  window_set_user_data(ctx->window, ctx);
  window_set_window_handlers(ctx->window, (WindowHandlers) {
    .load = pa_window_load,
    .unload = pa_window_unload,
  });
  window_set_background_color(ctx->window, GColorBlack);

  window_stack_push(ctx->window, true);
}
