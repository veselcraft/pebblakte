#include <pebble.h>
#include "vkontakte.h"
#include "sync.h"
#include "loading.h"
#include "error.h"
#include "dialogs.h"
#include "answers.h"
#include "messages.h"

static Window *s_window;
static SimpleMenuLayer *simple_menu_layer;
static SimpleMenuSection menu_sections[2];
static SimpleMenuItem menu_answers[MAX_ANSWERS];

char answers_titles[MAX_ANSWERS][20];
char* answers_texts[MAX_ANSWERS];
int answers_count;
bool waiting_answers = false;
extern int dialog_id;

#if PBL_COLOR
static DictationSession *s_dictation_session;
static char s_last_text[256];
static SimpleMenuItem menu_voice_answer[1];

static void dictation_session_callback(DictationSession *session, DictationSessionStatus status, 
                                       char *transcription, void *context) {
  if(status == DictationSessionStatusSuccess) {
    // Display the dictated text
    window_stack_pop(false);
    window_stack_pop(false);
    show_loading("Посылаем ответ...", true);
    sync_set_requested(PAGE_MESSAGES, dialog_id);

    int seq = rand();
	  DictionaryIterator *iter;
	  app_message_outbox_begin(&iter);
	  Tuplet tupleSeq = TupletInteger(MSG_KEY_SEQ, seq);
	  dict_write_tuplet(iter, &tupleSeq);
	  Tuplet tuple1 = TupletInteger(MSG_KEY_ANSWER_TO, dialog_id);
	  dict_write_tuplet(iter, &tuple1);
		Tuplet tuple2 = TupletCString(MSG_KEY_ANSWER_MESSAGE_CUSTOM, transcription);
		dict_write_tuplet(iter, &tuple2);
	  app_message_outbox_send();	    
  } else {
    // Display the reason for any error
    if (status == DictationSessionStatusFailureTranscriptionRejected) return;
    static char s_failed_buff[32];
    snprintf(s_failed_buff, sizeof(s_failed_buff), "Ошибка %d", (int)status);
    show_text_error(s_failed_buff);
  }
}

static void menu_voice_callback(int index, void *ctx) {
  dictation_session_start(s_dictation_session);
}
#endif

static void menu_select_callback(int index, void *ctx) {
  window_stack_pop(false);
  window_stack_pop(false);
  show_loading("Посылаем ответ...", true);
  sync_set_requested(PAGE_MESSAGES, dialog_id);
  send(0, MSG_KEY_ANSWER_TO, dialog_id, MSG_KEY_ANSWER_MESSAGE, index);  
}

void answers_set_title(int id, char *text)
{
  snprintf(answers_titles[id], 20, "Шаблон #%d", answers_count+1);
}

static void destroy_ui(void) {
  if (s_window)
    window_destroy(s_window);
  if (simple_menu_layer)
    simple_menu_layer_destroy(simple_menu_layer);
#if PBL_COLOR
  if (s_dictation_session)
    dictation_session_destroy(s_dictation_session);
#endif
}

static void handle_window_unload(Window* window) {
  destroy_ui();  
}

static void window_appear(Window* window)
{
}

static void window_disappear(Window* window)
{
}

void show_answers()
{
  int i;
#if PBL_COLOR
  if (!answers_count)
  {
    s_window = NULL;
    simple_menu_layer = NULL;    
    show_text_error("Нет вариантов ответов для отображения, задайте их в настройках.");
    return;
  }
#endif
  
  for (i = 0; i < answers_count; i++)
  {
    menu_answers[i] = (SimpleMenuItem) {
      .title = answers_titles[i],
      .subtitle = answers_texts[i],
      .callback = menu_select_callback
    };
  }

#if !PBL_COLOR
  int answers_section = 0;
#else
  int answers_section = 1;
  menu_voice_answer[0] = (SimpleMenuItem) {
    .title = "Слушать",
    .subtitle = NULL,
    .callback = menu_voice_callback
  };
  menu_sections[0] = (SimpleMenuSection) {
    .num_items = 1,
    .items = menu_voice_answer,
    .title = "Ответить голосом"
  };  
    // Create new dictation session
  s_dictation_session = dictation_session_create(sizeof(s_last_text)-1, dictation_session_callback, NULL);
  s_last_text[sizeof(s_last_text)-1] = 0;
#endif
  menu_sections[answers_section] = (SimpleMenuSection) {
    .num_items = answers_count,
    .items = menu_answers,
    .title = "Ответить шаблоном"
  };  
  
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
//  window_set_fullscreen(s_window, false);

  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_frame(window_layer);

  // Initialize the simple menu layer
#if !PBL_COLOR
  simple_menu_layer = simple_menu_layer_create(bounds, s_window, menu_sections, 1, NULL);
#else
  simple_menu_layer = simple_menu_layer_create(bounds, s_window, menu_sections, answers_count > 0 ? 2 : 1, NULL);
#endif

  // Add it to the window for display
  layer_add_child(window_layer, simple_menu_layer_get_layer(simple_menu_layer));  

  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
    .appear = window_appear,
    .disappear = window_disappear,
  });  
  
  hide_loading();
  window_stack_push(s_window, false);
}

void answers_received(int request_id)
{
  if (waiting_answers) show_answers();
}

void answers_request()
{
  if (!sync_is_page_loaded(PAGE_ANSWERS) && !sync_get_requested(PAGE_ANSWERS))
  {
    sync_set_requested(PAGE_ANSWERS, 1);
    send(0, MSG_KEY_REQUEST_LIST, 0, 0, 0);
  }
}
