#ifndef ARP_REPLY_WAITING_LIST_H
#define ARP_REPLY_WAITING_LIST_H

#include "assoc_array.h"
#include "sr_base_internal.h"
#include "sr_rt.h"
#include <pthread.h>
#include "eth_headers.h"
#include "fifo.h"

struct arwl_list_entry
{
    struct sr_packet * packet;
    uint32_t next_hop;
    char thru_interface[ETHER_ADDR_LEN];
};

struct arwl_entry
{
    uint32_t next_hop;
    struct fifo * list;
};

struct arp_reply_waiting_list
{
    struct assoc_array * array;
    pthread_mutex_t mutex;
};

struct arp_reply_waiting_list * arp_reply_waiting_list_create();
void arp_reply_waiting_list_add(struct arp_reply_waiting_list * list, struct sr_packet * packet, uint32_t next_hop, const char * thru_interface);
void arp_reply_waiting_list_dispatch(struct arp_reply_waiting_list * list, uint32_t ip);

#endif
