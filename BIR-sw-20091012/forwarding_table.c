#include "forwarding_table.h"
#include <stdlib.h>
#include <string.h>
#include "router.h"

void * forwarding_table_get_key(void * data)
{
    return &((struct forwarding_table_entry *)data)->dest;
}

struct forwarding_table * forwarding_table_create()
{
    NEW_STRUCT(ret,forwarding_table);
    ret->array_s = assoc_array_create(forwarding_table_get_key,assoc_array_key_comp_int);
    ret->array_d = assoc_array_create(forwarding_table_get_key,assoc_array_key_comp_int);
    ret->mutex = mutex_create();
    return ret;
}

void __delete_forwarding_table(void * data)
{
    free((struct forwarding_table_entry *)data);
}

void forwarding_table_destroy(struct forwarding_table * fwd_table)
{
    mutex_destroy(fwd_table->mutex);
    assoc_array_delete_array(fwd_table->array_s,__delete_forwarding_table);
    assoc_array_delete_array(fwd_table->array_d,__delete_forwarding_table);
    free(fwd_table);    
}

void forwarding_table_add_static_route(struct forwarding_table * fwd_table, struct sr_rt * rt_entry)
{
    NEW_STRUCT(e,forwarding_table_entry);
    mutex_lock(fwd_table->mutex);
    
    e->dest = rt_entry->dest.s_addr;
    e->gw   = rt_entry->gw.s_addr;
    e->mask = rt_entry->mask.s_addr;

    memcpy(e->interface,rt_entry->interface,SR_NAMELEN);

    assoc_array_insert(fwd_table->array_s,e);

    mutex_unlock(fwd_table->mutex);
}

struct __LPMSearch
{
    int found;
    int max_mask;
    uint32_t ip;
    uint32_t * next_hop;
    char * thru;
};

int __LPMSearchFn(void * data, void * user_data)
{
    struct __LPMSearch * srch = (struct __LPMSearch *) user_data;
    struct forwarding_table_entry * rt = (struct forwarding_table_entry *) data;
    int mask;

    mask = rt->mask;
    if((srch->ip & mask) == (rt->dest & mask))
    {
        if((mask & srch->max_mask) == srch->max_mask)
        {
            srch->found = 1;
            srch->max_mask = mask;
            *srch->next_hop = rt->gw;
            memcpy(srch->thru,rt->interface,SR_NAMELEN);
        }
    }
    return 0;
}

/*
 * Implementation of LPM (longest prefix match) search of forwarding table
 * */
int forwarding_table_lookup_next_hop(struct forwarding_table * fwd_table, uint32_t ip, uint32_t * next_hop, char * thru)
{
    struct __LPMSearch srch = {0,0,ip,next_hop,thru};
    mutex_lock(fwd_table->mutex);
    
    assoc_array_walk_array(fwd_table->array_d,__LPMSearchFn,(void*)&srch);
    if(srch.found == 0)
    {
        srch.max_mask = 0;
        assoc_array_walk_array(fwd_table->array_s,__LPMSearchFn,(void*)&srch);
    }
    if(*next_hop == 0)
    {
        *next_hop = ip;
    }

    mutex_unlock(fwd_table->mutex);

    return srch.found;
}
