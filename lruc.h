#include <pthread.h>
#include <stdint.h>
#include <time.h>

#ifndef __lruc_header__
#define __lruc_header__

// Error statuses
typedef enum {
  LRUC_NO_ERROR = 0,
  LRUC_MISSING_CACHE,
  LRUC_MISSING_KEY,
  LRUC_MISSING_VALUE,
  LRUC_PTHREAD_ERROR,
  LRUC_VALUE_TOO_LARGE
} STATUS;


// LRU cache item
typedef struct
{
  void *value;
  void *key;
  unsigned value_length;
  unsigned key_length;
  unsigned long access_count;
  void *next;
} lruc_item;

// LRU cache
typedef struct
{
  lruc_item **items;
  unsigned long access_count;
  unsigned long free_memory;
  unsigned long total_memory;
  unsigned long item_length;
  unsigned hash_table_size;
  time_t seed;
  lruc_item *free_items;
  pthread_mutex_t *mutex;
} lruc;


// Public functions
lruc *create_cache(unsigned long cache_size, unsigned item_length);
STATUS free_cache(lruc *cache);
STATUS set_item(lruc *cache, void *key, unsigned key_length, void *value, unsigned value_length);
STATUS get_item(lruc *cache, void *key, unsigned key_length, void **value);
STATUS delete_item(lruc *cache, void *key, unsigned key_length);

#endif

