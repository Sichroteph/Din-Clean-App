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
  uint8_t padding;  // empty rows at top for UP-opened root menus with few items
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

// Delayed return to avoid freeing menu context inside callback
static void delayed_return_to_watchface(void *data) {
  hub_return_to_watchface();
}

static void menu_window_load(Window *window) {
  MenuCtx *ctx = window_get_user_data(window);

  // Calculate padding for UP-opened root menus with few items
  ctx->padding = 0;
  if (ctx->depth == 0 && ctx->direction == HUB_DIR_UP && ctx->visible_count < 4) {
    ctx->padding = 4 - ctx->visible_count;
  }

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

  if (ctx->depth == 0 && ctx->direction == HUB_DIR_UP && ctx->visible_count > 0) {
    // Start at last real item (after padding), aligned to bottom for continuity
    MenuIndex last = { .section = 0, .row = ctx->padding + ctx->visible_count - 1 };
    menu_layer_set_selected_index(ctx->menu, last, MenuRowAlignBottom, false);
  } else if (ctx->depth > 0) {
    // Submenus: always start at first item, aligned to top
    MenuIndex first = { .section = 0, .row = 0 };
    menu_layer_set_selected_index(ctx->menu, first, MenuRowAlignTop, false);
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
  return ctx->visible_count + ctx->padding;
}

static void menu_draw_row(GContext *gctx, const Layer *cell,
                          MenuIndex *idx, void *data) {
  MenuCtx *ctx = data;

  // Padding rows are empty
  if (idx->row < ctx->padding) return;

  uint8_t real_row = idx->row - ctx->padding;
  uint8_t item_idx = ctx->visible_indices[real_row];
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

  // Ignore padding rows
  if (idx->row < ctx->padding) return;

  uint8_t real_row = idx->row - ctx->padding;
  uint8_t item_idx = ctx->visible_indices[real_row];
  const HubMenuItem *item = &ctx->all_items[item_idx];

  hub_timeout_reset();

  switch (item->type) {
    case HUB_MI_FOLDER:
      hub_menu_push_submenu(ctx->all_items, ctx->all_count,
                            item_idx, ctx->depth + 1, ctx->direction);
      break;
    case HUB_MI_PSEUDOAPP:
      vibes_double_pulse();
      hub_pseudoapp_push(item->data);
      break;
    case HUB_MI_ACTION:
      vibes_double_pulse();
      hub_action_execute(item->data);
      break;
  }
}

static void menu_selection_changed(MenuLayer *ml, MenuIndex new_index,
                                   MenuIndex old_index, void *data) {
  MenuCtx *ctx = data;
  hub_timeout_reset();

  // Exit-on-wrap only for root menus (depth == 0)
  if (ctx->depth != 0) return;

  uint16_t total_rows = ctx->visible_count + ctx->padding;
  uint16_t last_row = total_rows - 1;

  if (ctx->direction == HUB_DIR_DOWN) {
    // Opened by DOWN: exit when wrapping UP from first item
    if (old_index.row == 0 && new_index.row == last_row) {
      app_timer_register(0, delayed_return_to_watchface, NULL);
    }
  } else {
    // Opened by UP: exit when wrapping DOWN from last item
    if (old_index.row == last_row && new_index.row == 0) {
      app_timer_register(0, delayed_return_to_watchface, NULL);
    }
  }
}
