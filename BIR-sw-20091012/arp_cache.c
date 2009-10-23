#include "arp_cache.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fifo.h"
#include "lwtcp/lwip/sys.h"
#include "debug.h"

int arp_clear_cache(int key, void * data, void * user_data)
{
    struct arp_cache_entry * entry = (struct arp_cache_entry *) data;
    struct fifo * delete_list = (struct fifo *)user_data;

    entry->ttl -= 1;
    if(entry->ttl <= 0)
    {
        fifo_push(delete_list,entry);
    }
    
    return 0;
}

void arp_clear_cache_thread(void * arg)
{
    struct arp_cache * cache = (struct arp_cache *)arg;
    struct fifo * delete_list = fifo_create();
    struct arp_cache_entry * entry;

    while(1)
    {
        usleep(1000000);
        pthread_mutex_lock(&cache->mutex);
        assoc_array_walk_array(cache->array,arp_clear_cache,delete_list);
        while((entry = fifo_pop(delete_list)))
        {
            Debug("deleting ip ");dump_ip(entry->ip);Debug(" from ARP cache\n");
            free(assoc_array_delete(cache->array,entry->ip));
        }
        pthread_mutex_unlock(&cache->mutex);
    }
}

struct arp_cache * arp_cache_create()
{
    struct arp_cache * ret = (struct arp_cache *)malloc(sizeof(struct arp_cache));
    ret->array = assoc_array_create();
    pthread_mutex_init(&ret->mutex,NULL);

    sys_thread_new(arp_clear_cache_thread,ret);
    
    return ret;
}

int arp_cache_get_MAC_from_ip(struct arp_cache * cache, uint32_t ip, uint8_t * MAC)
{
    struct arp_cache_entry * res;
    int ret = 0;
    pthread_mutex_lock(&cache->mutex);

    res = assoc_array_read(cache->array,ip);
    if(res)
    {
        memcpy(MAC,res->MAC,ETHER_ADDR_LEN);
        ret = 1;
    }

    pthread_mutex_unlock(&cache->mutex);
    return ret;
}

void arp_cache_add(struct arp_cache * cache, uint32_t ip, const uint8_t * MAC)
{
    struct arp_cache_entry * entry = (struct arp_cache_entry *)malloc(sizeof(struct arp_cache_entry));
    pthread_mutex_lock(&cache->mutex);
    entry->ip = ip;
    entry->ttl = ARP_CACHE_TIMEOUT;
    memcpy(entry->MAC,MAC,ETHER_ADDR_LEN);
    if(assoc_array_read(cache->array,ip) == NULL)
    {
        assoc_array_insert(cache->array,ip,entry);
    }
    pthread_mutex_unlock(&cache->mutex);
}
