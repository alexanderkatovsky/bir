#include "forwarding_table.h"
#include <stdlib.h>
#include <string.h>
#include "router.h"

void * forwarding_table_get_key(void * data)
{
    return &((struct forwarding_table_entry *)data)->dest;
}

void forwarding_table_create(struct sr_instance * sr)
{
    NEW_STRUCT(ret,forwarding_table);
    ROUTER(sr)->fwd_table = ret;
    ret->array_s = assoc_array_create(forwarding_table_get_key,ip_address_cmp);
    ret->array_d = assoc_array_create(forwarding_table_get_key,ip_address_cmp);
    ret->mutex = mutex_create();
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

    e->dest.subnet = rt_entry->dest.s_addr;
    e->dest.mask   = rt_entry->mask.s_addr;
    e->next_hop    = rt_entry->gw.s_addr;

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

    mask = rt->dest.mask;
    if((srch->ip & mask) == (rt->dest.subnet & mask))
    {
        if((mask & srch->max_mask) == srch->max_mask)
        {
            srch->found = 1;
            srch->max_mask = mask;
            *srch->next_hop = rt->next_hop;
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
    if(srch.found == 0 || srch.max_mask == 0)
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

int forwarding_table_dynamic_entry_exists(struct forwarding_table * ft, struct ip_address * ip)
{
    return (assoc_array_read(ft->array_d, ip) != NULL);
}

void forwarding_table_add_dynamic(struct forwarding_table * ft, struct ip_address * ip,
                                  uint32_t next_hop, const char * interface)
{
    NEW_STRUCT(entry,forwarding_table_entry);
    entry->dest = *ip;
    entry->next_hop = next_hop;
    strcpy(entry->interface,interface);

    assoc_array_insert(ft->array_d,entry);
}

void forwarding_table_start_dijkstra(struct forwarding_table * fwd_table)
{
    assoc_array_delete_array(fwd_table->array_d,__delete_forwarding_table);
    fwd_table->array_d = assoc_array_create(forwarding_table_get_key,ip_address_cmp);

    mutex_lock(fwd_table->mutex);
}

void forwarding_table_end_dijkstra(struct forwarding_table * fwd_table)
{
    mutex_unlock(fwd_table->mutex);
}

int forwarding_table_show_a(void * data, void * userdata)
{
    struct forwarding_table_entry * fte = (struct forwarding_table_entry *)data;
    print_t print = (print_t)userdata;

    print_ip(fte->dest.subnet,print);print("\t");
    print_ip(fte->dest.mask,print);print("\t");
    print_ip(fte->next_hop,print);print("\t");
    print("%s\n",fte->interface);
    return 0;
}

void forwarding_table_dynamic_show(struct forwarding_table * ft, print_t print)
{
    print("Dynamic Forwarding Table:\n");
    
    mutex_lock(ft->mutex);
    assoc_array_walk_array(ft->array_d,forwarding_table_show_a,print);
    mutex_unlock(ft->mutex);
}

void forwarding_table_static_show(struct forwarding_table * ft, print_t print)
{
    print("Static Forwarding Table:\n");
    
    mutex_lock(ft->mutex);
    assoc_array_walk_array(ft->array_s,forwarding_table_show_a,print);
    mutex_unlock(ft->mutex);
}


struct __forwarding_table_static_loop_through_entries_i
{
    void (*fn)(struct forwarding_table_entry *, void *);
    void * userdata;
};

int __forwarding_table_static_loop_through_entries_a(void * data, void * userdata)
{
    struct forwarding_table_entry * fte = (struct forwarding_table_entry *)data;
    struct __forwarding_table_static_loop_through_entries_i * ftslte =
        (struct __forwarding_table_static_loop_through_entries_i *)userdata;
    ftslte->fn(fte,ftslte->userdata);
    return 0;
}

void forwarding_table_static_loop_through_entries(struct forwarding_table * ft,
                                                  void (*fn)(struct forwarding_table_entry *, void *),
                                                  void * userdata)
{
    struct __forwarding_table_static_loop_through_entries_i ftslte = {fn,userdata};
    mutex_lock(ft->mutex);
    assoc_array_walk_array(ft->array_s,__forwarding_table_static_loop_through_entries_a,&ftslte);
    mutex_unlock(ft->mutex);
}
