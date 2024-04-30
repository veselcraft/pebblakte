#include <pebble.h>
#include "vkontakte.h"
#include "sync.h"
#include "dialogs.h"
#include "messages.h"
#include "answers.h"

static int num_pages = 4;
static SyncPage pages[4];
static SyncElementInfo dialog_page_elements[1];
static SyncElementInfo dialog_row_elements[4];
static SyncElementInfo messages_row_elements[6];
static SyncElementInfo answers_row_elements[2];
static SyncElementInfo notifications_row_elements[1];

extern int dialogs_count;
extern int dialog_ids[];
extern char* dialog_titles[];
extern char* dialog_texts[];
extern bool notifications_enabled;

extern int messages_count;
extern char* messages_texts[];
extern char* messages_names[];
extern time_t messages_dates[];
extern int messages_read_state[];
extern int messages_out[];

extern char* answers_texts[];
extern int answers_count;

// Очищает элементы (все строки)
static void clear_elements(SyncElementInfo* element, int count, bool free_strings)
{
  int e;
  for (e = 0; e < count; e++)
  {
    //DEBUG "Row %d / %d", e, count);
    switch (element->data_type)
    {
      case INTEGER:
        //DEBUG "INTEGER");
        ((int*)element->data)[e] = 0;
        break;
      /*case STRING_SIMPLE:
        //DEBUG "STRING_SIMPLE");
        *((char*)element->data + e*element->size) = 0;
        break;
      */
      case STRING_DYNAMIC:
        //DEBUG "STRING_DYNAMIC");
        if (free_strings && ((char**)element->data)[e])
          free(((char**)element->data)[e]);
        ((char**)element->data)[e] = NULL;
        break;
      default:
        break;
    }
  }
}

// Очищает всю страницу
static void clear_page_data(SyncPage* page, bool free_strings)
{
  int e;
  for (e = 0; e < page->page_elements_count; e++)
  {
    //DEBUG "Clearing page element %d", e);
    clear_elements(&page->page_elements[e], 1, free_strings);
  }
  for (e = 0; e < page->row_elements_count; e++)
  {
    //DEBUG "Clearing row element %d", e);
    clear_elements(&page->row_elements[e], page->max_row_count, free_strings);
  }
  *page->counter = 0;
  page->start_received = false;
  page->end_received = false;
}

void free_page_data(int page_number)
{
  clear_page_data(&pages[page_number], true);
}

// Добавляет строку
char* add_string(char* p, char* s, int size)
{
  if (!s) s = "NULL";
  int len = strlen(s) + 1;
  int slen = p ? strlen(p) : 0;
  len += slen;
  if (len > size) len = size;
  if (p)
  {
    p = realloc(p, len);
    strncat(p, s, len-slen-1);
  } else {
    p = malloc(len);
    strncpy(p, s, len);
  }
  p[len-1] = 0;
  // unicode cut fix
  len = strlen(p);
  if ((len > 0) && (p[len-1] & 0xC0) == 0xC0)
    p[len-1] = 0;
  return p;
} 

// Записывает значение в заданное поле элемента
static void write_value(SyncElementInfo* element, int id, void* value)
{
  switch (element->data_type)
  {
    // Integer просто копируем (делать улитывать ли size?)
    case INTEGER:
      ((int*)element->data)[id] = *((int*)value);
      break;
    case STRING_DYNAMIC:
      STRING_ADDN(((char**)element->data)[id], (char*)value, element->size);
      break;
    case FUNCTION:
      if (element->data)
        ((AddDataCallback)element->data)(id, value);
      break;
  }  
}

void sync_init()
{
  //DEBUG "Init");
  memset(pages, 0, sizeof(pages));
  dialog_page_elements[0] = (SyncElementInfo) {
    .key = MSG_KEY_SHOW_NOTIFICATIONS,
    .data_type = INTEGER,
    .data = &notifications_enabled,
    .size = sizeof(int)
  };
  dialog_row_elements[0] = (SyncElementInfo) {
    .key = MSG_KEY_DIALOG_ID,
    .data_type = INTEGER,
    .data = dialog_ids,
    .size = sizeof(int)
  };
  dialog_row_elements[1] = (SyncElementInfo) {
    .key = MSG_KEY_DIALOG_OUT,
    .data_type = FUNCTION,
    .data = set_dialogs_out,
  };  
  dialog_row_elements[2] = (SyncElementInfo) {
    .key = MSG_KEY_DIALOG_TITLE,
    .data_type = STRING_DYNAMIC,
    .data = dialog_titles,
    .size = DIALOGS_TITLE_MAX_LENGTH
  };
  dialog_row_elements[3] = (SyncElementInfo) {
    .key = MSG_KEY_DIALOG_TEXT,
    .data_type = STRING_DYNAMIC,
    .data = dialog_texts,
    .size = DIALOGS_TEXT_MAX_LENGTH
  };
  
  messages_row_elements[0] = (SyncElementInfo) {
    .key = MSG_KEY_MESSAGES_OUT,
    .data_type = INTEGER,
    .data = messages_out,
    .size = sizeof(int)
  };
  messages_row_elements[1] = (SyncElementInfo) {
    .key = MSG_KEY_MESSAGES_TEXT,
    .data_type = STRING_DYNAMIC,
    .data = messages_texts,
    .size = MESSAGES_TEXT_MAX_LENGTH
  };
  messages_row_elements[2] = (SyncElementInfo) {
    .key = MSG_KEY_MESSAGES_DATE,
    .data_type = INTEGER,
    .data = messages_dates,
    .size = sizeof(int)
  };
  messages_row_elements[3] = (SyncElementInfo) {
    .key = MSG_KEY_MESSAGES_READ,
    .data_type = INTEGER,
    .data = messages_read_state,
    .size = sizeof(int)
  };
  messages_row_elements[4] = (SyncElementInfo) {
    .key = MSG_KEY_MESSAGES_NAME,
    .data_type = FUNCTION,
    .data = messages_set_name
  };
  messages_row_elements[5] = (SyncElementInfo) {
    .key = -1,
    .data_type = STRING_DYNAMIC,
    .data = messages_names
  };
  
  answers_row_elements[0] = (SyncElementInfo) {
    .key = MSG_KEY_ANSWERS_ELEMENT,
    .data_type = STRING_DYNAMIC,
    .data = answers_texts,
    .size = ANSWERS_MAX_LENGTH
  };
  answers_row_elements[1] = (SyncElementInfo) {
    .key = MSG_KEY_ANSWERS_ELEMENT,
    .data_type = FUNCTION,
    .data = answers_set_title
  };
  
  notifications_row_elements[0] = (SyncElementInfo) {
    .key = MSG_KEY_NOTIFICATIONS_TEXT,
    .data_type = STRING_DYNAMIC,
    .data = messages_texts,
    .size = MESSAGES_TEXT_MAX_LENGTH
  };
  
  pages[PAGE_DIALOGS] = (SyncPage) {
    .reset_key = MSG_KEY_DIALOGS_START,
    .element_end_key = MSG_KEY_DIALOG_TEXT,
    .page_end_key = MSG_KEY_DIALOG_END,
    .max_row_count = MAX_DIALOGS,
    .page_elements_count = 1,
    .page_elements = dialog_page_elements,
    .row_elements_count = sizeof(dialog_row_elements)/sizeof(SyncElementInfo),
    .row_elements = dialog_row_elements,
    .counter = &dialogs_count,
    .page_complited_callback = dialogs_received
  };
  pages[PAGE_MESSAGES] = (SyncPage) {
    .reset_key = MSG_KEY_MESSAGES_START,
    .element_end_key = MSG_KEY_MESSAGES_NAME,
    .page_end_key = MSG_KEY_MESSAGES_END,
    .max_row_count = MAX_MESSAGES,
    .page_elements_count = 0,
    .row_elements_count = sizeof(messages_row_elements)/sizeof(SyncElementInfo),
    .row_elements = messages_row_elements,
    .counter = &messages_count,
    .page_complited_callback = messages_received
  };  
  pages[PAGE_ANSWERS] = (SyncPage) {
    .reset_key = MSG_KEY_ANSWERS_START,
    .element_end_key = MSG_KEY_ANSWERS_ELEMENT,
    .page_end_key = MSG_KEY_ANSWERS_END,
    .max_row_count = MAX_ANSWERS,
    .page_elements_count = 0,
    .row_elements_count = sizeof(answers_row_elements)/sizeof(SyncElementInfo),
    .row_elements = answers_row_elements,
    .counter = &answers_count,
    .page_complited_callback = answers_received
  };
  pages[PAGE_NOTIFICATIONS] = (SyncPage) {
    .reset_key = MSG_KEY_NOTIFICATIONS_START,
    .element_end_key = MSG_KEY_NOTIFICATIONS_TEXT_END,
    .page_end_key = MSG_KEY_NOTIFICATIONS_END,
    .max_row_count = MAX_MESSAGES,
    .page_elements_count = 0,
    .row_elements_count = sizeof(notifications_row_elements)/sizeof(SyncElementInfo),
    .row_elements = notifications_row_elements,
    .counter = &messages_count,
    .page_complited_callback = messages_received
  };
  
  //DEBUG "Clearing");
  // Clearing
  int p;
  for (p = 0; p < num_pages; p++)
  {
    clear_page_data(&pages[p], false);
  }
  
  //DEBUG "Init OK");
}

void sync_deinit()
{
  int p;
  for (p = 0; p < num_pages; p++)
  {
    clear_page_data(&pages[p], true);
  }
}

static void sync_update_sniff_interval()
{
  bool reduced = false;
  int p;
  for (p = 0; p < num_pages; p++)
  {
    if (pages[p].requested) reduced = true;
  }
  
  if (reduced)
    app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
  else
    app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void sync_proceed_data(DictionaryIterator *received)
{
  //DEBUG "Received");
  int p, e;
  for (p = 0; p < num_pages; p++)
  {
    SyncPage* page = &pages[p];
    // Если не запрашивали ничего, то пропускаем
    if (!page->requested) continue;
    // Если старт страницы (ресет), то обнуляем все данные
    if (dict_find(received, page->reset_key))
    {
      //DEBUG "Reset");
      clear_page_data(page, true);
      page->start_received = true;
    }
    // Если не было команды на старт страницы (ресет), то пропускаем всю работу с данными
    if (!page->start_received) continue;
    
    for (e = 0; e < page->page_elements_count; e++)
    {
      Tuple* tuple = dict_find(received, page->page_elements[e].key);
      if (tuple)
      {
        //DEBUG "Value (global) for page %d, element %d", p, e);
        write_value(&page->page_elements[e], 0, tuple->value->cstring);
      }
    }
    if (*page->counter < page->max_row_count)
      for (e = 0; e < page->row_elements_count; e++)
      {
        Tuple* tuple = dict_find(received, page->row_elements[e].key);
        if (tuple)
        {
          //DEBUG "Value for page %d, element %d", p, e);
          write_value(&page->row_elements[e], *page->counter, tuple->value->cstring);
        }
      }
    if (dict_find(received, page->element_end_key) && (*page->counter < page->max_row_count))
    {
      //DEBUG "Element end");
      (*page->counter)++;
    }
    // Если получили конец страницы, и ID совпадает с запрошенным, вызываем callback функцию
    Tuple* tuple = dict_find(received, page->page_end_key);
    if (tuple && (tuple->value->int32 == page->requested) && page->page_complited_callback)
    {
      //DEBUG "Page end");
      page->end_received = true;
      page->page_complited_callback(page->requested);
      page->requested = 0;
      sync_update_sniff_interval();
    }
  }
}

void sync_set_requested(int page, int code)
{
  pages[page].requested = code;
  sync_update_sniff_interval();
}

int sync_get_requested(int page)
{
  return pages[page].requested;
}

bool sync_is_page_loaded(int page)
{
  return pages[page].end_received;
}
