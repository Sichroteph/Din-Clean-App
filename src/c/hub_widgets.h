#pragma once
#include "hub_config.h"

// Push a widget carousel window
// is_up: true = UP button's widget list, false = DOWN button's widget list
// direction: which button opened it (affects navigation direction)
void hub_widgets_push(bool is_up, HubDirection direction);
