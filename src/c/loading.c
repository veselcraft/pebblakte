#include <pebble.h>
#include "vkontakte.h"
#include "loading.h"
#include "error.h"

static Window *s_window = NULL;
static TextLayer *textlayer_loading;
static RotBitmapLayer* loading_layer;
static const char* please_wait = "Пожалуйста, подождите...";
static GBitmap *loading_bitmap;
static bool active = false;
static AppTimer *timer;

void update_bitmap_timer(void* data)
{
	rot_bitmap_layer_increment_angle(loading_layer, TRIG_MAX_ANGLE/8);
	timer = app_timer_register(100, (AppTimerCallback) update_bitmap_timer, NULL);
	if (!bluetooth_connection_service_peek())
	{
		hide_loading();
		show_communication_error(APP_MSG_NOT_CONNECTED);
	}
}

void set_text(char* text)
{
  if (!text)
		text_layer_set_text(textlayer_loading, please_wait);
  else 
		text_layer_set_text(textlayer_loading,  text);
}

static void initialise_ui(char* text) {
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
//  window_set_fullscreen(s_window, false);
  
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#if PBL_PLATFORM_CHALK
  int y_offset = 20;
#else
  int y_offset = 0;
#endif
  textlayer_loading = text_layer_create((GRect) { .origin = {10, 20 + y_offset}, .size = {SCREEN_WIDTH-20, 65} } );
  text_layer_set_background_color(textlayer_loading, GColorWhite);
  text_layer_set_text_color(textlayer_loading, GColorBlack);
  set_text(text);
  text_layer_set_text_alignment(textlayer_loading, GTextAlignmentCenter);
  text_layer_set_font(textlayer_loading, font);
  layer_add_child(window_get_root_layer(s_window), (Layer *)textlayer_loading);
	
	loading_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LOADING);
	loading_layer = rot_bitmap_layer_create(loading_bitmap);
	rot_bitmap_set_src_ic(loading_layer, GPoint(23, 23));
	GRect frame = layer_get_frame((Layer*)loading_layer);
	frame.origin.x = SCREEN_WIDTH/2 - frame.size.w/2;
	frame.origin.y = SCREEN_HEIGHT/2 - frame.size.h/2 + 20 + y_offset;
	layer_set_frame((Layer*)loading_layer, frame);	
  layer_add_child(window_get_root_layer(s_window), (Layer *)loading_layer);
}

static void destroy_ui(void) {
  active = false;
  window_destroy(s_window);
	s_window = NULL;
  text_layer_destroy(textlayer_loading);
  rot_bitmap_layer_destroy(loading_layer);  
  gbitmap_destroy(loading_bitmap);
}


static void handle_window_unload(Window* window) {
  destroy_ui();
}

static void window_appear(Window* window)
{
	timer = app_timer_register(200, (AppTimerCallback) update_bitmap_timer, NULL);
}

static void window_disappear(Window* window)
{
	app_timer_cancel(timer);
}

void show_loading(char* text, bool animated) {
  if (active)
  {
		set_text(text);
		return;
  }
  active = true;
  initialise_ui(text);
  window_set_window_handlers(s_window, (WindowHandlers) {
		.unload = handle_window_unload,
		.appear = window_appear,
		.disappear = window_disappear,
  });
	//window_stack_pop_all(false);
	window_stack_push(s_window, animated);
}

void update_text(char* text) {
  if (active)
  	set_text(text);	
}

void hide_loading(void) {
	if (s_window != NULL)
	{
		window_stack_remove(s_window, false);
	}
	active = false;
}
