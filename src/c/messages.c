#include <pebble.h>
#include "vkontakte.h"
#include "sync.h"
#include "loading.h"
#include "error.h"
#include "answers.h"
#include "messages.h"

static char* title = NULL;
int messages_count;
char* messages_texts[MAX_MESSAGES];
char* messages_names[MAX_MESSAGES];
time_t messages_dates[MAX_MESSAGES];
int messages_read_state[MAX_MESSAGES];
int messages_out[MAX_MESSAGES];

extern int dialog_ids[MAX_DIALOGS];
extern char* dialog_texts[MAX_DIALOGS];
extern int dialogs_count;

static char dates_text[MAX_MESSAGES][TIME_MAX_LENGTH];
int dialog_id = 0;
extern bool waiting_answers;

static Window *s_window;
static ScrollLayer *scroll_layer;
static TextLayer *text_layer_title;
static TextLayer *text_layer_element_time[MAX_MESSAGES];
static TextLayer *text_layer_element_name[MAX_MESSAGES];
static TextLayer *text_layer_element_text[MAX_MESSAGES];
static TextLayer *text_layer_no_elements;
static Layer *line_layer[MAX_MESSAGES];

void messages_set_title(char* t)
{
  FREE(title);
  if (!t) return;
  STRING_ADDN(title, t, DIALOGS_TITLE_MAX_LENGTH);
}

void messages_set_name(int id, char* name)
{
  if (messages_out[id]) {
    STRING_ADDN(messages_names[id], "Я:", DIALOGS_TITLE_MAX_LENGTH);
  } else {
    if (strlen(name) > 0)
      STRING_ADDN(messages_names[id], name, DIALOGS_TITLE_MAX_LENGTH);
    else
      STRING_ADDN(messages_names[id], title, DIALOGS_TITLE_MAX_LENGTH);
    STRING_ADDN(messages_names[id], ":", DIALOGS_TITLE_MAX_LENGTH);
  }
}

static void click_handler(ClickRecognizerRef recognizer, void *context) 
{
  if (dialog_id == 1) return;
  if (sync_is_page_loaded(PAGE_ANSWERS))
  {
    show_answers();
  }  else {
    waiting_answers = true;
    show_loading(NULL, true);
  }
}

static void config_provider(void *context)
{
  window_single_click_subscribe(BUTTON_ID_SELECT, click_handler);
}

static void line_draw(Layer *layer, GContext *ctx)
{ 
#if !PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorBlack);   
#else
  graphics_context_set_fill_color(ctx, GColorBlue); 
#endif
  graphics_fill_rect(ctx, GRect(0, 0, SCREEN_WIDTH, 2), 0, GCornerNone);
}

static void destroy_ui(void) {
  int element;
  text_layer_destroy(text_layer_title);
  for (element = 0; element < messages_count; element++)
  {
    if (dialog_id != 1)
    {
      text_layer_destroy(text_layer_element_time[element]);
      text_layer_destroy(text_layer_element_name[element]);
    }
    text_layer_destroy(text_layer_element_text[element]);
    layer_destroy(line_layer[element]);
  }
  if (!messages_count)
    text_layer_destroy(text_layer_no_elements);

  window_destroy(s_window);
  scroll_layer_destroy(scroll_layer);
}

static void handle_window_unload(Window* window) {
  destroy_ui();
  free_page_data(PAGE_MESSAGES);
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction)
{  
  window_stack_pop(false);
  show_loading("Обновляем...", true);
  if (dialog_id != 1)
  {
    sync_set_requested(PAGE_MESSAGES, dialog_id);
    send(0, MSG_KEY_REQUEST, dialog_id, 0, 0);
  } else {
    sync_set_requested(PAGE_NOTIFICATIONS, 1);
    send(0, MSG_KEY_REQUEST, 0, 0, 0);
  }
}

static void window_appear(Window* window)
{
  accel_tap_service_subscribe(accel_tap_handler);
  waiting_answers = false;
}

static void window_disappear(Window* window)
{
  accel_tap_service_unsubscribe();
}

void show_messages() 
{
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
//  window_set_fullscreen(s_window, false);

  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_frame(window_layer);
  scroll_layer = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(scroll_layer, s_window);

  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
    .appear = window_appear,
    .disappear = window_disappear,
  });
  
  GFont font_title = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  GFont font_element_time = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont font_element_title = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  GFont font_element_text = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  text_layer_title = text_layer_create((GRect) { .origin = {0, 0}, .size = {SCREEN_WIDTH, 15} } );  
  text_layer_set_font(text_layer_title, font_title);  
  text_layer_set_background_color(text_layer_title, GColorWhite);
  text_layer_set_text_color(text_layer_title, GColorBlack);  
  text_layer_set_text(text_layer_title, title);
  text_layer_set_text_alignment(text_layer_title, GTextAlignmentCenter);
  scroll_layer_add_child(scroll_layer, text_layer_get_layer(text_layer_title));  
  
  int y = text_layer_get_content_size(text_layer_title).h + 5;
  
  int element;
  int first_unread_line = 0;
  int last_message_line = 0;
  int unread_count = 0;
  char* last_message = NULL;
  int last_out = 0;

  for (element = messages_count-1; element >= 0; element--)
  {
    // some info about unread messages
    if (dialog_id != 1)
    {
      if (!messages_read_state[element]) unread_count++;
      if (!first_unread_line && !messages_read_state[element]) first_unread_line = y-1;
      last_message_line = y-1;
    }

    // Drawing line
    line_layer[element] = layer_create((GRect) { .origin = { 0, y }, .size = {SCREEN_WIDTH, 3} } );
    scroll_layer_add_child(scroll_layer, line_layer[element]);  
    layer_set_update_proc(line_layer[element], line_draw);
    y += 3;
  
    if (dialog_id != 1)
    {
      // Time
      text_layer_element_time[element] = text_layer_create((GRect) { .origin = {SCREEN_WIDTH - 39, y}, .size = {34, 30} } );  
      text_layer_set_font(text_layer_element_time[element], font_element_time);
      text_layer_set_background_color(text_layer_element_time[element], GColorClear);
      text_layer_set_text_color(text_layer_element_time[element], GColorBlack);    
      time_t t = messages_dates[element];
      struct tm * tt = localtime(&t);
      strftime(dates_text[element], TIME_MAX_LENGTH, "%H:%M", tt);
      text_layer_set_text(text_layer_element_time[element], dates_text[element]);
      text_layer_set_text_alignment(text_layer_element_time[element], GTextAlignmentRight);
      scroll_layer_add_child(scroll_layer, text_layer_get_layer(text_layer_element_time[element]));  
      //y += text_layer_get_content_size(text_layer_element_time[element]).h + 3;
  
      // Name
      text_layer_element_name[element] = text_layer_create((GRect) { .origin = {5, y}, .size = {100, 15} } );  
      text_layer_set_font(text_layer_element_name[element], font_element_title);
      text_layer_set_background_color(text_layer_element_name[element], GColorClear);
      text_layer_set_text_color(text_layer_element_name[element], GColorBlack);  
      text_layer_set_text(text_layer_element_name[element], messages_names[element]);
      text_layer_set_text_alignment(text_layer_element_name[element], GTextAlignmentLeft);
      scroll_layer_add_child(scroll_layer, text_layer_get_layer(text_layer_element_name[element]));  
      y += text_layer_get_content_size(text_layer_element_name[element]).h + 1;
    }
    
    // Text
    text_layer_element_text[element] = text_layer_create((GRect) { .origin = {3, y}, .size = {SCREEN_WIDTH-5, 1000} } );  
    text_layer_set_font(text_layer_element_text[element], font_element_text);
    text_layer_set_background_color(text_layer_element_text[element], GColorClear);
    text_layer_set_text_color(text_layer_element_text[element], GColorBlack);  
    text_layer_set_text(text_layer_element_text[element], messages_texts[element]);
    text_layer_set_text_alignment(text_layer_element_text[element], GTextAlignmentLeft);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(text_layer_element_text[element]));  
    y += text_layer_get_content_size(text_layer_element_text[element]).h + 7;
  
    if (dialog_id != 1) {
      last_message = messages_texts[element];
      last_out = messages_out[element];
    }
  }
  
  if (!messages_count)
  {
    GFont font_no_elements = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    text_layer_no_elements = text_layer_create((GRect) { .origin = { 5, 50 }, .size = {SCREEN_WIDTH-10, 60} } );  
    text_layer_set_font(text_layer_no_elements, font_no_elements);
    text_layer_set_background_color(text_layer_no_elements, GColorClear);
    text_layer_set_text_color(text_layer_no_elements, GColorBlack);
    text_layer_set_text(text_layer_no_elements, "Нет сообщений для отображения");
    text_layer_set_text_alignment(text_layer_no_elements, GTextAlignmentCenter);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(text_layer_no_elements));  
  }
  
  scroll_layer_set_content_size(scroll_layer, GSize(144, y));
  layer_add_child(window_layer, scroll_layer_get_layer(scroll_layer));
  scroll_layer_set_callbacks(scroll_layer, (ScrollLayerCallbacks) { .click_config_provider = config_provider });
  hide_loading();
  window_stack_push(s_window, false);

  if (dialog_id != 1)
  {
    // Moving to last unread message
    if (unread_count == 0) first_unread_line = last_message_line; // All read - to last message
    if (unread_count == messages_count) first_unread_line = 0; // All unread - going to up
    scroll_layer_set_content_offset(scroll_layer, GPoint(0,-first_unread_line), true);
  }
  
  int i;
  if (last_message)
  for (i = 0; i < dialogs_count; i++)
  {
    if (dialog_ids[i] == dialog_id)
    {
      FREE(dialog_texts[i]);
      if (last_out) STRING_ADDN(dialog_texts[i], "Я: ", DIALOGS_TEXT_MAX_LENGTH);
      STRING_ADDN(dialog_texts[i], last_message, DIALOGS_TEXT_MAX_LENGTH);
    }
  }
}

void messages_received(int request_id)
{
  dialog_id = request_id;
  show_messages();
  answers_request();
}
