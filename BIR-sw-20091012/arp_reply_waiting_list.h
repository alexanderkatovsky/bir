#ifndef ARP_REPLY_WAITING_LIST_H
#define ARP_REPLY_WAITING_LIST_H

#include "assoc_array.h"
#include "sr_base_internal.h"
#include "sr_rt.h"
#include <pthread.h>
#include "eth_headers.h"
#include "fifo.h"
#include "mutex.h"

#define ARP_REQUEST_MAX 5
#define ARP_REQUEST_TIMEOUT 3

struct arwl_list_entry
{
    struct sr_packet * packet;
    uint32_t next_hop;
};

struct arwl_entry
{
    struct sr_instance * sr;
    uint32_t next_hop;
    char thru_interface[ETHER_ADDR_LEN];
    struct fifo * packet_list;

    uint8_t request_number;
    uint8_t request_ttl;
};

struct arp_reply_waiting_list
{
    struct assoc_array * array;
    struct sr_mutex * mutex;
    int exit_signal;
};

struct arp_reply_waiting_list * arp_reply_waiting_list_create();
void arp_reply_waiting_list_destroy(struct arp_reply_waiting_list * list);
void arp_reply_waiting_list_add(struct arp_reply_waiting_list * list, struct sr_packet * packet,
                                uint32_t next_hop, const char * thru_interface);
void arp_reply_waiting_list_dispatch(struct arp_reply_waiting_list * list, uint32_t ip);

void arp_request_handler_process_reply(struct sr_packet * packet);
void arp_request_handler_make_request(struct sr_packet * packet, uint32_t next_hop, char * thru_interface);

#endif
