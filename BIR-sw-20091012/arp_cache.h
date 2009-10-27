#ifndef ARP_CACHE_H
#define ARP_CACHE_H

#include <pthread.h>
#include "sr_base_internal.h"
#include "eth_headers.h"
#include "assoc_array.h"
#include "debug.h"

#define ARP_CACHE_TIMEOUT 15

struct arp_cache_entry
{
    uint32_t ip;
    char MAC[ETHER_ADDR_LEN];

    int ttl;
};

struct arp_cache
{
    struct assoc_array * array;

    pthread_mutex_t mutex;
    int signal;
};

struct arp_cache * arp_cache_create();
void arp_cache_destroy(struct arp_cache * cache);
int arp_cache_get_MAC_from_ip(struct arp_cache * cache, uint32_t ip, uint8_t * MAC);
void arp_cache_add(struct arp_cache * cache, uint32_t ip, const uint8_t * MAC);

void arp_cache_show(struct arp_cache * cache,print_t print);

#endif
