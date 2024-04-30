#include <pebble.h>
#include "vkontakte.h"
#include "sync.h"
#include "loading.h"
#include "error.h"
#include "dialogs.h"
#include "messages.h"
#include "answers.h"

int seq = -1;

void send(int seq, int key1, int value1, int key2, int value2)
{
	if (!seq) seq = rand();
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	Tuplet tupleSeq = TupletInteger(MSG_KEY_SEQ, seq);
	dict_write_tuplet(iter, &tupleSeq);
	Tuplet tuple1 = TupletInteger(key1, value1);
	dict_write_tuplet(iter, &tuple1);
	if (key2)
	{
		Tuplet tuple2 = TupletInteger(key2,  value2);
		dict_write_tuplet(iter, &tuple2);
	}
	app_message_outbox_send();	
}

static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	if (reason == APP_MSG_SEND_TIMEOUT) // resend
	{
//		APP_LOG(APP_LOG_LEVEL_ERROR, "Timeout, resend");
		int seq = 0;
		Tuple* tuple = dict_find(failed, MSG_KEY_SEQ);
		if (tuple)
		{
			seq = tuple->value->int32;
		}
		int key1 = 0;
		int value1 = 0;
		int key2 = 0;
		int value2 = 0;
		int i = 0;
		for (i = 1; i < 50; i++)
		{
			tuple = dict_find(failed, i);
			if (tuple && !key1)
			{
				key1 = i;
				value1 = tuple->value->int32;
			} else if (tuple && !key2)
			{
				key2 = i;
				value2 = tuple->value->int32;
			}
		}
		if (key1) send(seq, key1, value1, key2, value2);
	} 
	else show_communication_error(reason);
}

static void in_received_handler(DictionaryIterator *received, void *context) {
  // incoming message received
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Data!");

	// На случай перезапуска скрипта
	if (dict_find(received, MSG_KEY_JAVASCRIPT_STARTED)) seq = -1;

	Tuple* tuple = dict_find(received, MSG_KEY_SEQ);
	if (tuple)
	{
		int received_seq = tuple->value->int32;
		if ((seq > 0) && (received_seq != seq+1)) {
			APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid seq: %d is not %d", (int)tuple->value->int32, (int)(seq+1));
			return;
		}		
		seq = tuple->value->int32;
	}

  tuple = dict_find(received, MSG_KEY_STATE);
  if (tuple)
	{
    switch (tuple->value->int8)
		{
			case STATE_REQUEST:
				update_text("Выполняем запрос...");
				break;
			case STATE_RECEIVING:
				update_text("Получаю данные...");
				break;
		}
	}
	
	tuple = dict_find(received, MSG_KEY_CRITICAL_ERROR);
  if (tuple)
	{
		show_text_error(tuple->value->cstring);
	}

	tuple = dict_find(received, MSG_KEY_VK_ERROR);
  if (tuple)
	{
		show_vk_error(tuple->value->int32);
	}

	sync_proceed_data(received);
}

static void init(void) {
	sync_init();
	sync_set_requested(PAGE_DIALOGS, 1);
  app_message_register_inbox_received(in_received_handler);
  app_message_register_outbox_failed(out_failed_handler);
  app_message_open(INBOUND_SIZE, OUTBOUND_SIZE);
}

static void deinit(void) {
	free_page_data(PAGE_ANSWERS);
	messages_set_title(NULL);
}

int main(void) {
  init();

	show_loading(NULL, true);

	app_event_loop();
	
  deinit();
}
