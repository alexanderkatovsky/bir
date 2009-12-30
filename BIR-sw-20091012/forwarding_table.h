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
    struct ip_address ip;
    uint32_t next_hop;
    char   interface[SR_NAMELEN];
};

struct forwarding_table_subnet_list
{
    struct assoc_array * list;
    uint32_t mask;
};

struct forwarding_table
{
    struct assoc_array * array_s;
    struct assoc_array * array_d;
    struct assoc_array * array_nat;
    struct sr_mutex * mutex;

    int running_dijkstra;
};

void forwarding_table_create(struct sr_instance * sr);
void forwarding_table_destroy(struct forwarding_table * fwd_table);
int forwarding_table_lookup_next_hop(struct forwarding_table * fwd_table, uint32_t ip,
                                     uint32_t * next_hop, char * thru, int nat);
int forwarding_table_dynamic_entry_exists(struct forwarding_table * ft, struct ip_address * ip, int nat);
void forwarding_table_add(struct sr_instance * sr, struct ip_address * ip,
                          uint32_t next_hop, char * interface, int isDynamic, int nat);
int forwarding_table_remove(struct sr_instance * sr, struct ip_address * ip, int isDynamic, int nat);
void forwarding_table_start_dijkstra(struct sr_instance * sr);
void forwarding_table_end_dijkstra(struct sr_instance * sr);
void forwarding_table_dynamic_show(struct forwarding_table * ft, print_t print, int nat);
void forwarding_table_static_show(struct forwarding_table * ft, print_t print);

void forwarding_table_loop(struct forwarding_table * ft,
                           void (*fn)(uint32_t,uint32_t,uint32_t,char*,void*,int*),
                           void * userdata, int isDynamic, int nat);
#endif

