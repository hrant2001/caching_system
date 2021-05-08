#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>
#include "lruc.h"

#define CACHE_SIZE  (8 * 1024)  // 8k
#define AVG_SIZE    (2 * 1024)  // 2k

char *keys[5] = {
  "key0",
  "key1",
  "key2",
  "key3",
  "key4"
};

char *values[5] = {
  "value0",
  "value1",
  "value2",
  "value3",
  "value4"  
};

void copy_key_val(lruc *cache, char *key, char *val)
{
  int error = 0;
  char *new_key = (char *) malloc(strlen(key) + 1);
  strcpy(new_key, key);
  char *new_val = (char *) malloc(strlen(val) + 1);
  strcpy(new_val, val);
  if(error = set_item(cache, new_key, strlen(key) + 1, new_val, AVG_SIZE))
    errx(1, "Error in set: %i\n", error);
}

int main()
{
  int error = 0;
  char *get = NULL;
  
  // Create a new cache
  printf("Creating a cache...\n");
  lruc *cache = create_cache(CACHE_SIZE, AVG_SIZE);
  
  // Insert the five values 1000 times
  printf("Setting values...\n");
  for(int i = 0; i < 1000; i++)
  {
    	copy_key_val(cache, keys[i % 5], values[i % 5]);
  }
  
  // We should be able to get 4th, 3rd, 2nd, 1st, but not 0th
  printf("Getting values...\n");
  for(int i = 4; i >= 0; i--)
  {
    printf("Getting: %s\n", keys[i]);
    if(error = get_item(cache, keys[i], strlen(keys[i]) + 1, (void **)(&get)))
      errx(1, "Error in get: %i\n", error);

    if(get)
      printf("Value returned was: %s\n", get);
    else
      printf("No value was returned\n");
  }
  
  printf("Freeing the cache...\n");
  if(error = free_cache(cache))
    errx(1, "Error in free: %i\n", error);
  printf("Cache is freed!\n");
  
  return 0;
}
