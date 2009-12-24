#ifndef __DHCP_H
#define __DHCP_H

#include "assoc_array.h"
#include "common.h"
#include "mutex.h"
#include "debug.h"
#include "eth_headers.h"

struct sr_instance;

struct sr_packet;

struct dhcp_s
{
    char name[SR_NAMELEN];
    uint32_t from;
    uint32_t to;
    uint32_t mask;
};

void * dhcp_s_get_key(void * data);

struct dhcp_table_key
{
    char interface[SR_NAMELEN];
    struct ip_address ip;
};

struct dhcp_table_entry
{
    struct dhcp_table_key key;
    uint8_t MAC[ETHER_ADDR_LEN];
};

struct dhcp_table
{
    struct bi_assoc_array * array;

    struct sr_mutex * mutex;
};

int dhcp_packet(struct sr_packet * packet);
void dhcp_handle_incoming(struct sr_packet * packet);

void dhcp_create(struct sr_instance * sr);
void dhcp_destroy(struct dhcp_table * dhcp);

#endif
