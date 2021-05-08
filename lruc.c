#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lruc.h"

// Private functions
unsigned hash_func(lruc *cache, void *key, unsigned key_length)
{
  unsigned m = 0x5bd1e995;
  unsigned r = 24;
  unsigned h = cache->seed ^ key_length;
  char* data = (char *)key;

  while(key_length >= 4)
  {
    unsigned k = *(unsigned *)data;
    k *= m;
    k ^= k >> r;
    k *= m;
    h *= m;
    h ^= k;
    data += 4;
    key_length -= 4;
  }
  
  switch(key_length)
  {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
            h *= m;
  };

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;
  return h % cache->hash_table_size;
}

// Compare a key against an existing item's key
int compare_keys(lruc_item *item, void *key, unsigned key_length)
{
  if(key_length != item->key_length)
    return 1;
  else
    return memcmp(key, item->key, key_length);
}

// Remove an item and push it to the free items queue
void remove_item(lruc *cache, lruc_item *prev, lruc_item *item, unsigned hash_index)
{
  if(prev)
    prev->next = item->next;
  else
    cache->items[hash_index] = (lruc_item *) item->next;
  
  // Free the memory and update the free memory counter
  cache->free_memory += item->value_length;
  free(item->value);
  free(item->key);
  
  // Push the item to the free items queue 
  memset(item, 0, sizeof(lruc_item));
  item->next = cache->free_items;
  cache->free_items = item;
}

// Remove the least recently used item
void remove_lru_item(lruc *cache)
{
  lruc_item *min_item = NULL, *min_prev = NULL;
  lruc_item *item = NULL, *prev = NULL;
  unsigned min_index = -1;
  unsigned long min_access_count = -1;
  
  for(unsigned i = 0; i < cache->hash_table_size; i++)
  {
    item = cache->items[i];
    prev = NULL;
    
    while(item)
    {
      if(item->access_count < min_access_count || min_access_count == -1)
      {
        min_access_count = item->access_count;
        min_item  = item;
        min_prev  = prev;
        min_index = i;
      }
      prev = item;
      item = item->next;
    }
  }
  
  if(min_item)
    remove_item(cache, min_prev, min_item, min_index);
}

// Pop an existing item from the free queue, or create a new one
lruc_item *pop_or_create_item(lruc *cache)
{
  lruc_item *item = NULL;
  
  if(cache->free_items)
  {
    item = cache->free_items;
    cache->free_items = item->next;
  } else {
    item = (lruc_item *) calloc(sizeof(lruc_item), 1);
  }
  
  return item;
}

/////////////////////////////////////////////////////////////////

// Public functions
lruc *create_cache(unsigned long cache_size, unsigned item_length) 
{

  lruc *cache = (lruc *) calloc(sizeof(lruc), 1);
  if(!cache)
  {
    perror("LRU Cache: Unable to create a cache");
    return NULL;
  }
  cache->hash_table_size = cache_size / item_length;
  cache->item_length = item_length;
  cache->free_memory = cache_size;
  cache->total_memory = cache_size;
  cache->seed = time(NULL);
  
  cache->items = (lruc_item **) calloc(sizeof(lruc_item *), cache->hash_table_size);
  if(!cache->items)
  {
    perror("LRU Cache: Unable to create a cache hash table");
    free(cache);
    return NULL;
  }
  
  // All cache calls are guarded by a mutex
  cache->mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
  if(pthread_mutex_init(cache->mutex, NULL))
  {
    perror("LRU Cache: Unable to initialise a mutex");
    free(cache->items);
    free(cache);
    return NULL;
  }
  return cache;
}

STATUS free_cache(lruc *cache) 
{  
  // Free each of the cached items, and the hash table
  lruc_item *item = NULL, *next = NULL;
  
  if(cache->items) 
  {
    for(unsigned i = 0; i < cache->hash_table_size; i++)
    {
      item = cache->items[i];    
      while(item)
      {
        next = (lruc_item *) item->next;
        free(item);
        item = next;
      }
    }
    free(cache->items);
  }
  
  // Free the cache
  if(cache->mutex) {
    if(pthread_mutex_destroy(cache->mutex))
    {
      perror("LRU Cache: Unable to destroy the mutex");
      return LRUC_PTHREAD_ERROR;
    }
  }
  free(cache);
  
  return LRUC_NO_ERROR;
}

STATUS set_item(lruc *cache, void *key, unsigned key_length, void *value, unsigned value_length)
{  
  // See if the key already exists
  unsigned hash_index = hash_func(cache, key, key_length);
  unsigned required = 0;
  lruc_item *item = NULL;
  lruc_item *prev = NULL;
  item = cache->items[hash_index];
  
  while(item && compare_keys(item, key, key_length))
  {
    prev = item;
    item = (lruc_item *) item->next;
  }
  
  if(item)
  {
    // Update the value and the value_lengths
    required = value_length - item->value_length;
    free(item->value);
    item->value = value;
    item->value_length = value_length;
    
  } 
  else 
  {
    // Insert a new item
    item = pop_or_create_item(cache);
    item->value = value;
    item->key = key;
    item->value_length = value_length;
    item->key_length = key_length;
    required = value_length;
    
    if(prev)
      prev->next = item;
    else
      cache->items[hash_index] = item;
 }
  item->access_count = ++cache->access_count;
  
  // Remove as many items as necessary to free enough space
  if(required > 0 && required > cache->free_memory)
  {
    while(cache->free_memory < required)
      remove_lru_item(cache);
  }
  cache->free_memory -= required;

  return LRUC_NO_ERROR;
}

STATUS get_item(lruc *cache, void *key, unsigned key_length, void **value)
{  
  // Loop until we find the item, or hit the end
  unsigned hash_index = hash_func(cache, key, key_length);
  lruc_item *item = cache->items[hash_index];
  
  while(item && compare_keys(item, key, key_length))
    item = (lruc_item *) item->next;
  
  if(item)
  {
    *value = item->value;
    item->access_count = ++cache->access_count;
  } 
  else
  {
    *value = NULL;
  }
  
  return LRUC_NO_ERROR;
}
