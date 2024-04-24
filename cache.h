#include <stdio.h> 
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct _Cache
{
  struct _Cache *prev;
  struct _Cache *next;
  char *data; // response
  char key[MAXLINE]; // path
  size_t size; // content length
} Cache_t;

Cache_t *find_cache(char *key);
void insert_cache(char *key, char *data, int size);
int delete_cache(int size, int total, char *key);