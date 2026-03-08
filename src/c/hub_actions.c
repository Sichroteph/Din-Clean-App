#include "hub_actions.h"

typedef struct {
  Window *window;
  TextLayer *text_layer;
  AppTimer *close_timer;
} ActionCtx;

static void action_window_load(Window *window);
static void action_window_unload(Window *window);
static void action_close_timer_cb(void *context);

void hub_action_execute(uint8_t webhook_index) {
  // Send webhook trigger to JS side
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, KEY_HUB_WEBHOOK, webhook_index);
    app_message_outbox_send();
  }

  // Show confirmation window
  ActionCtx *ctx = malloc(sizeof(ActionCtx));
  if (!ctx)
    return;

  ctx->close_timer = NULL;
  ctx->window = window_create();
  window_set_user_data(ctx->window, ctx);
  window_set_window_handlers(ctx->window, (WindowHandlers){
                                              .load = action_window_load,
                                              .unload = action_window_unload,
                                          });
  window_set_background_color(ctx->window, GColorBlack);

  window_stack_push(ctx->window, true);
}

static void action_window_load(Window *window) {
  ActionCtx *ctx = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  ctx->text_layer = text_layer_create(GRect(0, 60, bounds.size.w, 40));
  text_layer_set_text(ctx->text_layer, "Action sent!");
  text_layer_set_font(ctx->text_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(ctx->text_layer, GTextAlignmentCenter);
  text_layer_set_background_color(ctx->text_layer, GColorBlack);
  text_layer_set_text_color(ctx->text_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(ctx->text_layer));

  // Auto-close and return to watchface after 2 seconds
  ctx->close_timer = app_timer_register(2000, action_close_timer_cb, ctx);
}

static void action_close_timer_cb(void *context) {
  ActionCtx *ctx = context;
  ctx->close_timer = NULL;
  // Return to watchface (pops all windows above main)
  hub_return_to_watchface();
}

static void action_window_unload(Window *window) {
  ActionCtx *ctx = window_get_user_data(window);
  if (ctx->close_timer) {
    app_timer_cancel(ctx->close_timer);
  }
  text_layer_destroy(ctx->text_layer);
  window_destroy(ctx->window);
  free(ctx);
}
