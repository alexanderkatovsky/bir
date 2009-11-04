#ifndef FORWARDING_TABLE_H
#define FORWARDING_TABLE_H

#include "assoc_array.h"
#include "sr_base_internal.h"
#include "sr_rt.h"
#include "mutex.h"
#include "common.h"
#include "debug.h"

#define FTABLE_STATIC_ENTRY  0
#define FTABLE_DYNAMIC_ENTRY 1

struct forwarding_table_entry
{
    struct ip_address dest;
    uint32_t next_hop;
    char   interface[SR_NAMELEN];
};

struct forwarding_table
{
    struct assoc_array * array_s;
    struct assoc_array * array_d;
    struct sr_mutex * mutex;
};

struct forwarding_table * forwarding_table_create();
void forwarding_table_destroy(struct forwarding_table * fwd_table);
void forwarding_table_add_static_route(struct forwarding_table * fwd_table, struct sr_rt * rt_entry);
int forwarding_table_lookup_next_hop(struct forwarding_table * fwd_table, uint32_t ip, uint32_t * next_hop, char * thru);
int forwarding_table_dynamic_entry_exists(struct forwarding_table * ft, struct ip_address * ip);
void forwarding_table_add_dynamic(struct forwarding_table * ft, struct ip_address * ip,
                                  uint32_t next_hop, const char * interface);
void forwarding_table_start_dijkstra(struct forwarding_table * ft);
void forwarding_table_end_dijkstra(struct forwarding_table * fwd_table);
void forwarding_table_dynamic_show(struct forwarding_table * ft, print_t print);

#endif

