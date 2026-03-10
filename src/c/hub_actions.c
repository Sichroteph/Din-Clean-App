#include "hub_actions.h"

// Zero-heap toast flag: read by update_proc in Din_Clean.c
bool g_hub_action_toast = false;

void hub_action_execute(uint8_t webhook_index) {
  // Send webhook trigger to JS side
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, KEY_HUB_WEBHOOK, webhook_index);
    app_message_outbox_send();
  }
  // Visual confirmation: backlight flash (haptic already provided by caller).
  // No Window/TextLayer allocated — avoids ~130B transient heap pressure and
  // the timing race between focus-change and window unload.
  light_enable_interaction();
  g_hub_action_toast = true;
}
