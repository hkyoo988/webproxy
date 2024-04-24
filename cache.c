#include "cache.h"
#include "stdlib.h"
#include "string.h"

Cache_t *head = NULL;
Cache_t *tail = NULL;

Cache_t *find_cache(char *key){
    Cache_t *node = head;
    while(node != NULL){
        if(!strcmp(node->key, key)){
            if(node == head){
                return node;
            }
            else if(node == tail){
                tail = node->prev;
                node->prev->next = NULL;
            }else{
                node->prev->next = node->next;
                node->next->prev = node->prev;
            }
            node->next = head;
            node->prev = NULL;
            head = node;
            return node;
        }        
        node = node->next;
    }
    return node;
}

void insert_cache(char *key, char *data, int size){
    Cache_t *new_cache = (Cache_t *)malloc(sizeof(Cache_t));
    if (new_cache == NULL) {
        // 메모리 할당 실패 처리
        return;
    }
    char *response = (char *)malloc(size);
    if (response == NULL) {
        // 메모리 할당 실패 처리
        free(new_cache);
        return;
    }
    memcpy(response, data, size);

    if(head == NULL){
        tail = new_cache;
        new_cache->next = NULL;
    }else{
        new_cache->next = head;
        new_cache->next->prev = new_cache;
    }
    new_cache->prev = NULL;

    strncpy(new_cache->key, key, MAXLINE);
    new_cache->data = response;
    new_cache->size = size;
    
    head = new_cache;

    return;
}

int delete_cache(int size, int total, char *key){
    if (tail == NULL) {
        // 캐시가 비어있을 경우 처리
        return 0; // 실패를 나타내는 플래그나 에러 코드 반환
    }
    
    Cache_t *ptr = tail;
    while(ptr != NULL){
        tail->prev->next = NULL;
        tail = tail->prev;
        total -= ptr->size;
        free(ptr->data);
        free(ptr);
        ptr = tail;

        if(total+size <= MAX_CACHE_SIZE){
            return total+size;
        }
    }
    return size;
}
