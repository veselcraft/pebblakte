#include <pebble.h>

#define STRING_ADDN(p, s, n) p = add_string(p, s, n)
#define STRING_ADD(p, s) p = add_string(p, s, 1000)
#define FREE(s) if (s) {free(s); s=NULL;}

typedef void (*PageComplitedCallback)(int id);

typedef void (*AddDataCallback)(int id, void* data);

typedef enum {
	INTEGER,
	//STRING_SIMPLE,
	STRING_DYNAMIC,
	FUNCTION
} SyncDataType;

typedef struct SyncElementInfo {
	int key;
	SyncDataType data_type;
	void *data;
	int size;
} SyncElementInfo;


typedef struct SyncPage {
	int reset_key; // ID сообщений, которое обнуляет все данные
	int element_end_key; // ID сообщений, которое указывает, что закончилась строка данных
	int page_end_key; // ID сообщений, которое указывает, что передана вся страница
	int max_row_count; // Максимальное количество строк
	int page_elements_count; // Количество элементов, которые просто относятся к таблице, а не к строкам (опционально)
	SyncElementInfo* page_elements; // Элементы, которые просто относятся к таблице, а не к строкам (опционально)
	int row_elements_count; // Количество элементов строк
	SyncElementInfo* row_elements; // Элементы строк
	int* counter; // Указатель на счётчик элементов
	PageComplitedCallback page_complited_callback;
	int requested; // Флаг указывающий, что мы что-то запрашивали и содержащий ID запроса.
	bool start_received; // Указывает, получали ли мы начало страницы
	bool end_received; // Указывает, получили ли мы конец страницы
} SyncPage;

char* add_string(char* p, char* s, int size);
void sync_init();
void sync_deinit();
void sync_proceed_data(DictionaryIterator *received);
void sync_set_requested(int page, int code);
int sync_get_requested(int page);
bool sync_is_page_loaded(int page);
void free_page_data(int page_number);
void strncpy_my(char* tp, char* ts, int size);