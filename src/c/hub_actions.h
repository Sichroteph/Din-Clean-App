#pragma once
#include "hub_config.h"

// Set to true when an action was just executed; cleared by main layer after 1.5s.
// Lives in BSS — zero heap cost.
extern bool g_hub_action_toast;

// Sends the webhook for the given index and arms the toast notification.
// Does NOT push any window — caller is responsible for returning to watchface
// if needed (use hub_return_to_watchface() or a deferred timer for safety).
void hub_action_execute(uint8_t webhook_index);
