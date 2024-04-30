#include <pebble.h>
#include "vkontakte.h"
#include "sync.h"
#include "dialogs.h"
#include "loading.h"
#include "error.h"
#include "messages.h"

static Window *s_window;
static SimpleMenuLayer *simple_menu_layer;
static SimpleMenuSection menu_sections[2];
static SimpleMenuItem menu_notifications[1];
static SimpleMenuItem menu_dialogs[MAX_DIALOGS];
int dialog_ids[MAX_DIALOGS];
char* dialog_titles[MAX_DIALOGS];
char* dialog_texts[MAX_DIALOGS];
int dialogs_count;
int notifications_enabled;

static void notifications_callback(int index, void *ctx) {
  show_loading(NULL, true);
  messages_set_title("Ответы");
  sync_set_requested(PAGE_NOTIFICATIONS, 1);
  send(0, MSG_KEY_REQUEST, 0, 0, 0);
}

static void menu_select_callback(int index, void *ctx) {
  show_loading(NULL, true);
  sync_set_requested(PAGE_MESSAGES, dialog_ids[index]);
  messages_set_title(dialog_titles[index]);
  send(0, MSG_KEY_REQUEST, dialog_ids[index], 0, 0);
}

void set_dialogs_out(int id, void* data)
{
  if (*((int*)data))
  {
    STRING_ADDN(dialog_texts[id], "Я: ", DIALOGS_TEXT_MAX_LENGTH);
  }
}

static void destroy_ui(void) {
  if (s_window)
    window_destroy(s_window);
  if (simple_menu_layer)
    simple_menu_layer_destroy(simple_menu_layer);
}

static void handle_window_unload(Window* window) {
  destroy_ui();
  free_page_data(PAGE_DIALOGS);
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction)
{
  window_stack_pop_all(false);
  show_loading("Обновляем...", true);
  sync_set_requested(PAGE_DIALOGS, 1);
  send(0, MSG_KEY_REFRESH_DIALOGS, 0, 0, 0);
}

static void window_appear(Window* window)
{
  sync_set_requested(PAGE_MESSAGES, 0); // Cancel any request
  sync_set_requested(PAGE_NOTIFICATIONS, 0); // Cancel any request
  accel_tap_service_subscribe(accel_tap_handler);
  
  // Обновление
  int i;
  for (i = 0; i < dialogs_count; i++)
  {
    menu_dialogs[i] = (SimpleMenuItem) {
      .title = dialog_titles[i],
      .subtitle = dialog_texts[i],
      .callback = menu_select_callback
    };
  }
}

static void window_disappear(Window* window)
{
  accel_tap_service_unsubscribe();
}

void show_dialogs()
{
  int i;
  if (!dialogs_count)
  {
    s_window = NULL;
    simple_menu_layer = NULL;    
    show_text_error("Нет диалогов для отображения");
    return;
  }

  menu_notifications[0] = (SimpleMenuItem) {
    .title = "Ответы",
    .subtitle = "Уведомления",
    .callback = notifications_callback
  };

  for (i = 0; i < dialogs_count; i++)
  {
    menu_dialogs[i] = (SimpleMenuItem) {
      .title = dialog_titles[i],
      .subtitle = dialog_texts[i],
      .callback = menu_select_callback
    };
  }

  if (!notifications_enabled) {
    menu_sections[0] = (SimpleMenuSection) {
      .num_items = dialogs_count,
      .items = menu_dialogs,
      //.title = "Диалоги"
    };
  } else {
    menu_sections[0] = (SimpleMenuSection) {
      .num_items = 1,
      .items = menu_notifications,
    };  
    menu_sections[1] = (SimpleMenuSection) {
      .num_items = dialogs_count,
      .items = menu_dialogs,
      .title = "Диалоги"
    };  
  }
  
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
//  window_set_fullscreen(s_window, false);

  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_frame(window_layer);

  // Initialize the simple menu layer
  simple_menu_layer = simple_menu_layer_create(bounds, s_window, menu_sections, notifications_enabled ? 2 : 1, NULL);

  // Add it to the window for display
  layer_add_child(window_layer, simple_menu_layer_get_layer(simple_menu_layer));  
  
  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
    .appear = window_appear,
    .disappear = window_disappear,
  });
  
  window_stack_pop_all(false);
  window_stack_push(s_window, false);
}

void hide_dialogs(void)
{
  window_stack_remove(s_window, true);
}

void dialogs_received(int request_id)
{
  show_dialogs();
}
