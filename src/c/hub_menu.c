#include "hub_menu.h"
#include "hub_pseudoapp.h"
#include "hub_actions.h"

typedef struct {
  Window *window;
  MenuLayer *menu;
  const HubMenuItem *all_items;
  uint8_t all_count;
  uint8_t visible_indices[HUB_MAX_MENU_ITEMS];
  uint8_t visible_count;
  int8_t parent_index;
  uint8_t depth;
  HubDirection direction;
} MenuCtx;

// Forward declarations
static uint16_t menu_get_num_rows(MenuLayer *ml, uint16_t section, void *data);
static void menu_draw_row(GContext *ctx, const Layer *cell,
                          MenuIndex *idx, void *data);
static void menu_select(MenuLayer *ml, MenuIndex *idx, void *data);
static void menu_selection_changed(MenuLayer *ml, MenuIndex new_index,
                                   MenuIndex old_index, void *data);
static void menu_window_load(Window *window);
static void menu_window_unload(Window *window);

void hub_menu_push(bool is_up_menu, HubDirection direction) {
  uint8_t count;
  const HubMenuItem *items;

  if (is_up_menu) {
    items = hub_config_get_up_menu(&count);
  } else {
    items = hub_config_get_down_menu(&count);
  }

  if (!items || count == 0) return;

  hub_menu_push_submenu(items, count, -1, 0, direction);
}

void hub_menu_push_submenu(const HubMenuItem *items, uint8_t count,
                           int8_t parent_index, uint8_t depth,
                           HubDirection direction) {
  if (depth >= HUB_MAX_MENU_DEPTH) return;

  MenuCtx *ctx = malloc(sizeof(MenuCtx));
  if (!ctx) return;

  ctx->all_items = items;
  ctx->all_count = count;
  ctx->parent_index = parent_index;
  ctx->depth = depth;
  ctx->direction = direction;

  // Find visible items (children of parent_index)
  ctx->visible_count = hub_menu_get_children(
    items, count, parent_index, ctx->visible_indices, HUB_MAX_MENU_ITEMS);

  if (ctx->visible_count == 0) {
    free(ctx);
    return;
  }

  ctx->window = window_create();
  window_set_user_data(ctx->window, ctx);
  window_set_window_handlers(ctx->window, (WindowHandlers) {
    .load = menu_window_load,
    .unload = menu_window_unload,
  });

  window_stack_push(ctx->window, true);
}

static void menu_window_load(Window *window) {
  MenuCtx *ctx = window_get_user_data(window);

  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  ctx->menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(ctx->menu, ctx, (MenuLayerCallbacks) {
    .get_num_rows = menu_get_num_rows,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
    .selection_changed = menu_selection_changed,
  });
  menu_layer_set_click_config_onto_window(ctx->menu, window);
  layer_add_child(root, menu_layer_get_layer(ctx->menu));

  // If opened via UP, start at last item
  if (ctx->direction == HUB_DIR_UP && ctx->visible_count > 0) {
    MenuIndex last = { .section = 0, .row = ctx->visible_count - 1 };
    menu_layer_set_selected_index(ctx->menu, last, MenuRowAlignCenter, false);
  }

  hub_timeout_reset();
}

static void menu_window_unload(Window *window) {
  MenuCtx *ctx = window_get_user_data(window);
  menu_layer_destroy(ctx->menu);
  window_destroy(ctx->window);
  free(ctx);
}

static uint16_t menu_get_num_rows(MenuLayer *ml, uint16_t section, void *data) {
  MenuCtx *ctx = data;
  return ctx->visible_count;
}

static void menu_draw_row(GContext *gctx, const Layer *cell,
                          MenuIndex *idx, void *data) {
  MenuCtx *ctx = data;
  uint8_t item_idx = ctx->visible_indices[idx->row];
  const HubMenuItem *item = &ctx->all_items[item_idx];

  const char *subtitle = NULL;
  switch (item->type) {
    case HUB_MI_FOLDER:    subtitle = ">"; break;
    case HUB_MI_PSEUDOAPP: break;
    case HUB_MI_ACTION:    subtitle = "!"; break;
  }

  menu_cell_basic_draw(gctx, cell, item->label, subtitle, NULL);
}

static void menu_select(MenuLayer *ml, MenuIndex *idx, void *data) {
  MenuCtx *ctx = data;
  uint8_t item_idx = ctx->visible_indices[idx->row];
  const HubMenuItem *item = &ctx->all_items[item_idx];

  hub_timeout_reset();

  switch (item->type) {
    case HUB_MI_FOLDER:
      hub_menu_push_submenu(ctx->all_items, ctx->all_count,
                            item_idx, ctx->depth + 1, ctx->direction);
      break;
    case HUB_MI_PSEUDOAPP:
      hub_pseudoapp_push(item->data);
      break;
    case HUB_MI_ACTION:
      hub_action_execute(item->data);
      break;
  }
}

static void menu_selection_changed(MenuLayer *ml, MenuIndex new_index,
                                   MenuIndex old_index, void *data) {
  hub_timeout_reset();
}
