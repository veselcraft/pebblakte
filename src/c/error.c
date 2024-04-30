#include <pebble.h>
#include "vkontakte.h"
#include "error.h"
#include "loading.h"
#include "sync.h"

static Window *s_window;
static TextLayer *textlayer_error;
static TextLayer *textlayer_error2;
static char* error_text = NULL;
static bool active = false;

static void initialise_ui_communication(AppMessageResult reason) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Error: %d", reason);
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
//  window_set_fullscreen(s_window, false);
  
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont font_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  // textlayer_error
  textlayer_error = text_layer_create((GRect) { .origin = {0, 30}, .size = {SCREEN_WIDTH, 65} } );
  textlayer_error2 = text_layer_create((GRect) { .origin = {0, 60}, .size = {SCREEN_WIDTH, 100} } );
  text_layer_set_background_color(textlayer_error, GColorBlack);
  text_layer_set_background_color(textlayer_error2, GColorBlack);
  text_layer_set_text_color(textlayer_error, GColorWhite);
  text_layer_set_text_color(textlayer_error2, GColorWhite);
  text_layer_set_text(textlayer_error, "Ошибка");
  if (reason != 0)
  {
		if (reason == APP_MSG_SEND_TIMEOUT)
			text_layer_set_text(textlayer_error2, "Таймаут. Ответ от телефона не получен.");
		if (reason == APP_MSG_SEND_REJECTED)
			text_layer_set_text(textlayer_error2, "Запрос отклонён.");
		if (reason == APP_MSG_NOT_CONNECTED)
			text_layer_set_text(textlayer_error2, "Нет подключения к телефону.");
		if (reason == APP_MSG_APP_NOT_RUNNING)
			text_layer_set_text(textlayer_error2, "Приложение не запущено.");
  }

  text_layer_set_text_alignment(textlayer_error, GTextAlignmentCenter);
  text_layer_set_text_alignment(textlayer_error2, GTextAlignmentCenter);
  text_layer_set_font(textlayer_error, font_bold);
  text_layer_set_font(textlayer_error2, font);
  layer_add_child(window_get_root_layer(s_window), (Layer *)textlayer_error);
  layer_add_child(window_get_root_layer(s_window), (Layer *)textlayer_error2);
}

static void initialise_ui_text(char* text) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Error: %s", text);
	FREE(error_text);
	error_text = malloc(strlen(text)+1);
	strcpy(error_text, text);
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
//  window_set_fullscreen(s_window, false);
  
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont font_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  // textlayer_error
  textlayer_error = text_layer_create((GRect) { .origin = {0, 30}, .size = {SCREEN_WIDTH, 61} } );
  textlayer_error2 = text_layer_create((GRect) { .origin = {0, 60}, .size = {SCREEN_WIDTH, 100} } );
  text_layer_set_background_color(textlayer_error, GColorBlack);
  text_layer_set_background_color(textlayer_error2, GColorBlack);
  text_layer_set_text_color(textlayer_error, GColorWhite);
  text_layer_set_text_color(textlayer_error2, GColorWhite);
  text_layer_set_text(textlayer_error, "Ошибка");
	text_layer_set_text(textlayer_error2, error_text);
  text_layer_set_text_alignment(textlayer_error, GTextAlignmentCenter);
  text_layer_set_text_alignment(textlayer_error2, GTextAlignmentCenter);
  text_layer_set_font(textlayer_error, font_bold);
  text_layer_set_font(textlayer_error2, font);
  layer_add_child(window_get_root_layer(s_window), (Layer *)textlayer_error);
  layer_add_child(window_get_root_layer(s_window), (Layer *)textlayer_error2);
}

static void initialise_ui_vk(int code) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
//  window_set_fullscreen(s_window, false);
  
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont font_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  // textlayer_error
  textlayer_error = text_layer_create((GRect) { .origin = {0, 30}, .size = {SCREEN_WIDTH, 65} } );
  textlayer_error2 = text_layer_create((GRect) { .origin = {0, 60}, .size = {SCREEN_WIDTH, 100} } );
  text_layer_set_background_color(textlayer_error, GColorBlack);
  text_layer_set_background_color(textlayer_error2, GColorBlack);
  text_layer_set_text_color(textlayer_error, GColorWhite);
  text_layer_set_text_color(textlayer_error2, GColorWhite);
  text_layer_set_text(textlayer_error, "Ошибка");	
	static char *result;
  switch (code)
  {
		case 1:
			result = "Неизвестная ошибка.";
			break;
		case 2:
			result = "Приложение выключено.";
			break;
		case 3:
			result = "Передан неизвестный метод.";
			break;
		case 4:
			result = "Неверная подпись.";
			break;			
		case 5:
			result = "Авторизация пользователя не удалась. Проверьте правильность токена.";
			break;			
		case 6:
			result = "Слишком много запросов в секунду.";
			break;
		case 7:
			result = "Нет прав для выполнения этого действия.";
			break;
		case 8:
			result = "Неверный запрос.";
			break;
		case 9:
			result = "Слишком много однотипных действий.";
			break;
		case 10:
			result = "Произошла внутренняя ошибка сервера.";
			break;
		case 11:
			result = "В тестовом режиме приложение должно быть выключено или пользователь должен быть залогинен.";
			break;
		case 14:
			result = "Требуется ввод кода с картинки (Captcha).";
			break;
		case 15:
			result = "Доступ запрещён.";
			break;
		case 16:
			result = "Требуется выполнение запросов по протоколу HTTPS, т.к. пользователь включил настройку, требующую работу через безопасное соединение.";
			break;
		case 17:
			result = "Требуется валидация пользователя.";
			break;
		case 20:
			result = "Данное действие запрещено для не Standalone приложений.";
			break;
		case 21:
			result = "Данное действие разрешено только для Standalone и Open API приложений.";
			break;
		case 23:
			result = "Метод был выключен.";
			break;
		case 100:
			result = "Один из необходимых параметров был не передан или неверен.";
			break;
		case 101:
			result = "Неверный API ID приложения.";
			break;
		case 113:
			result = "Неверный идентификатор пользователя.";
			break;
		case 150:
			result = "Неверный timestamp.";
			break;
		case 200:
			result = "Доступ к альбому запрещён.";
			break;
		case 201:
			result = "Доступ к аудио запрещён.";
			break;
		case 203:
			result = "Доступ к группе запрещён.";
			break;
		case 300:
			result = "Альбом переполнен.";
			break;
		case 500:
			result = "Действие запрещено. Вы должны включить переводы голосов в настройках приложения.";
			break;
		case 600:
			result = "Нет прав на выполнение данных операций с рекламным кабинетом.";
			break;
		case 603:
			result = "Произошла ошибка при работе с рекламным кабинетом.";
			break;			
		default:
			FREE(error_text);
			error_text = malloc(50);
			snprintf(error_text, 50, "Код ошибки: %d", code);
			result = error_text;
			break;
  }
  APP_LOG(APP_LOG_LEVEL_ERROR, "VK error: %s", result);
	if (result) text_layer_set_text(textlayer_error2, result);

  text_layer_set_text_alignment(textlayer_error, GTextAlignmentCenter);
  text_layer_set_text_alignment(textlayer_error2, GTextAlignmentCenter);
  text_layer_set_font(textlayer_error, font_bold);
  text_layer_set_font(textlayer_error2, font);
  layer_add_child(window_get_root_layer(s_window), (Layer *)textlayer_error);
  layer_add_child(window_get_root_layer(s_window), (Layer *)textlayer_error2);
}

static void destroy_ui(void) {
  window_destroy(s_window);
  text_layer_destroy(textlayer_error);
  text_layer_destroy(textlayer_error2);
}


static void handle_window_unload(Window* window) {
	active = false;
  destroy_ui();
	FREE(error_text);
}

static void init_main()
{
	active = true;
  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
  });
  //window_stack_pop_all(false);
	hide_loading();
  window_stack_push(s_window, true);
}

void show_communication_error(AppMessageResult reason) {
	if (active) return;
  initialise_ui_communication(reason);
	init_main();
}

void show_text_error(char* text)
{
	if (active) return;
	if (strcmp(text, "NOTOKEN") == 0)
	{
		show_text_error("Откройте настойки приложения на телефоне и введите токен.");
		return;
	}
	initialise_ui_text(text);
  init_main();
}

void show_vk_error(int code)
{
	if (active) return;
	initialise_ui_vk(code);
  init_main();
}

void hide_error(void) {
  window_stack_remove(s_window, true);
}
