#ifndef ARP_CACHE_H
#define ARP_CACHE_H

#include <pthread.h>
#include "sr_base_internal.h"
#include "eth_headers.h"
#include "bi_assoc_array.h"
#include "debug.h"
#include "mutex.h"

struct sr_packet;

#define ARP_CACHE_TIMEOUT 15

struct arp_cache_entry
{
    uint32_t ip;
    uint8_t MAC[ETHER_ADDR_LEN];

    int ttl;
};

struct arp_cache
{
    struct bi_assoc_array * array;
    struct bi_assoc_array * array_s;    

    struct sr_mutex * mutex;
    int exit_signal;
};

void arp_cache_create(struct sr_instance * sr);
void arp_cache_destroy(struct arp_cache * cache);
int arp_cache_get_MAC_from_ip(struct arp_cache * cache, uint32_t ip, uint8_t * MAC);
void arp_cache_add(struct sr_instance * sr, uint32_t ip, const uint8_t * MAC, int isDynamic);
int arp_cache_remove_entry(struct sr_instance * sr, uint32_t ip, int isDynamic);
int arp_cache_purge(struct sr_instance * sr, int isDynamic);

void arp_cache_show(struct arp_cache * cache,print_t print);
void arp_cache_alert_packet_received(struct sr_packet * packet);
#endif
