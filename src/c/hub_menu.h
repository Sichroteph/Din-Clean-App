#pragma once
#include "hub_config.h"

// Push a menu window
// is_up_menu: true = UP button's menu, false = DOWN button's menu
// direction: which button opened it (affects initial selection)
void hub_menu_push(bool is_up_menu, HubDirection direction);

// Push a sub-menu for a folder item (used internally and by menu select)
void hub_menu_push_submenu(const HubMenuItem *items, uint8_t count,
                           int8_t parent_index, uint8_t depth,
                           HubDirection direction);
