#ifndef LINK_STATE_GRAPH_H
#define LINK_STATE_GRAPH_H

#include "common.h"
#include "assoc_array.h"
#include "pwospf_protocol.h"
#include "sr_base_internal.h"
#include "linked_list.h"
#include "debug.h"

struct dijkstra
{
    struct ip_address address;
    uint32_t rid;
    uint32_t next_hop;
    char interface[SR_NAMELEN];
};


struct link
{
    struct ip_address ip;
    uint32_t rid;
};

struct link_state_node
{
    uint32_t rid;
    struct assoc_array * links;
    int seq;
};

struct link_state_graph
{
    /*router id --> list of {subnet, mask, router id}*/
    struct assoc_array * array;

    struct sr_mutex * mutex;
};

void link_state_graph_destroy(struct link_state_graph * lsg);
void link_state_graph_create(struct sr_instance * sr);
int link_state_graph_update_links(struct sr_instance * sr,
                                  uint32_t rid, uint16_t seq, uint32_t num,
                                  struct ospfv2_lsu * adv);
void link_state_graph_update_forwarding_table(struct sr_instance * sr);
void link_state_graph_show_topology(struct link_state_graph * lsg, print_t print);

void link_state_graph_loop_links(struct sr_instance * sr, void (*fn)(uint32_t rid_node, int seq,
                                                                     uint32_t rid_link,
                                                                     struct ip_address, void *), void * userdata);


#endif

