#ifndef NEIGHBOUR_LIST
#define NEIGHBOUR_LIST

#include "assoc_array.h"
#include "mutex.h"
#include "sr_base_internal.h"
#include "eth_headers.h"
#include "debug.h"


struct neighbour
{
    uint32_t router_id;
    uint32_t aid;
    uint32_t ip;
    uint32_t nmask;
    uint16_t helloint;

    uint16_t ttl;
};

struct neighbour_list
{
    struct assoc_array * array;
    
    struct sr_mutex * mutex;
};

void neighbour_list_destroy(struct neighbour_list * list);
struct neighbour_list * neighbour_list_create();
void neighbour_list_scan_neighbours(struct sr_instance * sr, struct neighbour_list * nl);
int neighbour_list_process_incoming_hello(struct sr_instance * sr, struct neighbour_list * n_list,
                                           uint32_t ip,uint32_t rid,uint32_t aid,uint32_t nmask,uint16_t helloint);

void neighbour_list_loop(struct neighbour_list * nlist, void (*fn)(struct neighbour *, void *), void * userdata);
int neighbour_list_empty(struct neighbour_list * nlist);
#endif
