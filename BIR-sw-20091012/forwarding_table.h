#ifndef FORWARDING_TABLE_H
#define FORWARDING_TABLE_H

#include "assoc_array.h"
#include "sr_base_internal.h"
#include "sr_rt.h"
#include <pthread.h>

#define FTABLE_STATIC_ENTRY  0
#define FTABLE_DYNAMIC_ENTRY 1

struct forwarding_table_entry
{
    uint32_t dest;
    uint32_t gw;
    uint32_t mask;
    char   interface[SR_NAMELEN];

    uint8_t type;
};

struct forwarding_table
{
    struct assoc_array * array;
    pthread_mutex_t mutex;
};

struct forwarding_table * forwarding_table_create();
void forwarding_table_add_static_route(struct forwarding_table * fwd_table, struct sr_rt * rt_entry);
int forwarding_table_lookup_next_hop(struct forwarding_table * fwd_table, uint32_t ip, uint32_t * next_hop, char * thru);


#endif

