#include "hub_menu.h"
#include "hub_actions.h"
#include "hub_pseudoapp.h"

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
  uint8_t padding; // empty rows at top for UP-opened root menus with few items
} MenuCtx;

// Forward declarations
static uint16_t menu_get_num_rows(MenuLayer *ml, uint16_t section, void *data);
static void menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx,
                          void *data);
static void menu_select(MenuLayer *ml, MenuIndex *idx, void *data);
static void menu_selection_changed(MenuLayer *ml, MenuIndex new_index,
                                   MenuIndex old_index, void *data);
static void menu_window_load(Window *window);
static void menu_window_unload(Window *window);
static void handle_up_click(ClickRecognizerRef recognizer, void *context);
static void handle_down_click(ClickRecognizerRef recognizer, void *context);
static void handle_select_click(ClickRecognizerRef recognizer, void *context);
static void menu_click_config_provider(void *context);

void hub_menu_push(bool is_up_menu, HubDirection direction) {
  uint8_t count;
  const HubMenuItem *items;

  if (is_up_menu) {
    items = hub_config_get_up_menu(&count);
  } else {
    items = hub_config_get_down_menu(&count);
  }

  if (!items || count == 0)
    return;

  hub_menu_push_submenu(items, count, -1, 0, direction);
}

void hub_menu_push_submenu(const HubMenuItem *items, uint8_t count,
                           int8_t parent_index, uint8_t depth,
                           HubDirection direction) {
  if (depth >= HUB_MAX_MENU_DEPTH)
    return;

  MenuCtx *ctx = malloc(sizeof(MenuCtx));
  if (!ctx)
    return;

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
  window_set_window_handlers(ctx->window, (WindowHandlers){
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
  if (ctx->depth == 0 && ctx->direction == HUB_DIR_UP &&
      ctx->visible_count < 4) {
    ctx->padding = 4 - ctx->visible_count;
  }

  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  ctx->menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(ctx->menu, ctx,
                           (MenuLayerCallbacks){
                               .get_num_rows = menu_get_num_rows,
                               .draw_row = menu_draw_row,
                               .select_click = menu_select,
                               .selection_changed = menu_selection_changed,
                           });
  window_set_click_config_provider_with_context(
      window, menu_click_config_provider, ctx);
  layer_add_child(root, menu_layer_get_layer(ctx->menu));

  if (ctx->depth == 0 && ctx->direction == HUB_DIR_UP &&
      ctx->visible_count > 0) {
    // Start at last real item (after padding), aligned to bottom for continuity
    MenuIndex last = {.section = 0,
                      .row = ctx->padding + ctx->visible_count - 1};
    menu_layer_set_selected_index(ctx->menu, last, MenuRowAlignBottom, false);
  } else if (ctx->depth > 0) {
    // Submenus: always start at first item, aligned to top
    MenuIndex first = {.section = 0, .row = 0};
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

static void menu_draw_row(GContext *gctx, const Layer *cell, MenuIndex *idx,
                          void *data) {
  MenuCtx *ctx = data;

  // Padding rows are empty
  if (idx->row < ctx->padding)
    return;

  uint8_t real_row = idx->row - ctx->padding;
  uint8_t item_idx = ctx->visible_indices[real_row];
  const HubMenuItem *item = &ctx->all_items[item_idx];

  GRect bounds = layer_get_bounds(cell);

  // Set drawing color based on selection state
  MenuIndex sel = menu_layer_get_selected_index(ctx->menu);
  bool is_selected = (sel.section == idx->section && sel.row == idx->row);
  GColor fg = is_selected ? GColorWhite : GColorBlack;

  graphics_context_set_fill_color(gctx, fg);
  graphics_context_set_stroke_color(gctx, fg);

  // Draw a 14×14 type icon at x=4, vertically centred
  const int ICON_W = 14;
  const int ICON_X = 4;
  int icon_y = (bounds.size.h - ICON_W) / 2;

  switch (item->type) {
  case HUB_MI_FOLDER:
    // Tab: 5×2 at top-left of icon area
    graphics_fill_rect(gctx, GRect(ICON_X, icon_y, 5, 2), 0, GCornerNone);
    // Body: outline rectangle, 1 px below the tab
    graphics_draw_rect(gctx, GRect(ICON_X, icon_y + 1, ICON_W, ICON_W - 1));
    break;

  case HUB_MI_PSEUDOAPP:
    // App icon: rounded-square outline with a filled centre mark
    graphics_draw_round_rect(gctx, GRect(ICON_X, icon_y, ICON_W, ICON_W), 3);
    graphics_fill_rect(gctx, GRect(ICON_X + 5, icon_y + 5, 4, 4), 0,
                       GCornerNone);
    break;

  case HUB_MI_ACTION:
    // Quick action: right-pointing filled triangle (▶)
    // Each column drawn as a vertical segment that shrinks toward the tip
    for (int col = 0; col < ICON_W / 2; col++) {
      graphics_draw_line(gctx, GPoint(ICON_X + col, icon_y + col),
                         GPoint(ICON_X + col, icon_y + ICON_W - 1 - col));
    }
    break;
  }

  // Label text, offset to the right of the icon, vertically centred
  int text_x = ICON_X + ICON_W + 4;
  GRect text_rect =
      GRect(text_x, (bounds.size.h - 22) / 2, bounds.size.w - text_x, 26);
  graphics_context_set_text_color(gctx, fg);
  graphics_draw_text(
      gctx, item->label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      text_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void menu_select(MenuLayer *ml, MenuIndex *idx, void *data) {
  MenuCtx *ctx = data;

  // Ignore padding rows
  if (idx->row < ctx->padding)
    return;

  uint8_t real_row = idx->row - ctx->padding;
  uint8_t item_idx = ctx->visible_indices[real_row];
  const HubMenuItem *item = &ctx->all_items[item_idx];

  hub_timeout_reset();

  switch (item->type) {
  case HUB_MI_FOLDER:
    hub_menu_push_submenu(ctx->all_items, ctx->all_count, item_idx,
                          ctx->depth + 1, ctx->direction);
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
}

// Custom click handlers replace menu_layer_set_click_config_onto_window so we
// can detect when the user presses the opposite button at the list boundary
// and exit immediately, without relying on wrap (MenuLayer doesn't wrap).

static void handle_up_click(ClickRecognizerRef recognizer, void *context) {
  MenuCtx *ctx = context;
  hub_timeout_reset();
  MenuIndex current = menu_layer_get_selected_index(ctx->menu);
  uint16_t first_real_row = (uint16_t)ctx->padding;

  if (ctx->depth == 0 && ctx->direction == HUB_DIR_DOWN) {
    // Menu opened by DOWN: pressing UP at first real item exits
    if (current.row <= first_real_row) {
      app_timer_register(0, delayed_return_to_watchface, NULL);
      return;
    }
  }
  // Navigate up, but not past first real row
  if (current.row > first_real_row) {
    MenuIndex prev = {.section = 0, .row = current.row - 1};
    menu_layer_set_selected_index(ctx->menu, prev, MenuRowAlignCenter, true);
  }
}

static void handle_down_click(ClickRecognizerRef recognizer, void *context) {
  MenuCtx *ctx = context;
  hub_timeout_reset();
  MenuIndex current = menu_layer_get_selected_index(ctx->menu);
  uint16_t last_real_row = (uint16_t)(ctx->padding + ctx->visible_count - 1);

  if (ctx->depth == 0 && ctx->direction == HUB_DIR_UP) {
    // Menu opened by UP: pressing DOWN at last real item exits
    if (current.row >= last_real_row) {
      app_timer_register(0, delayed_return_to_watchface, NULL);
      return;
    }
  }
  // Navigate down, but not past last real row
  if (current.row < last_real_row) {
    MenuIndex next = {.section = 0, .row = current.row + 1};
    menu_layer_set_selected_index(ctx->menu, next, MenuRowAlignCenter, true);
  }
}

static void handle_select_click(ClickRecognizerRef recognizer, void *context) {
  MenuCtx *ctx = context;
  MenuIndex idx = menu_layer_get_selected_index(ctx->menu);
  menu_select(ctx->menu, &idx, ctx);
}

static void menu_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, handle_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, handle_down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, handle_select_click);
}
