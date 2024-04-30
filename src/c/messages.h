#include <pebble.h>
/*
void messages_request(int id);
void messages_reset();
void messages_set_out_date_name(int out, time_t date, int read, char* name);
void messages_add_text(char* text);
*/
void messages_set_title(char* t);
void messages_set_name(int id, char* text);
void show_messages();
void messages_received(int request_id);