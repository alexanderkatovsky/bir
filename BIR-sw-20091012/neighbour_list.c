#include "neighbour_list.h"
#include <stdlib.h>
#include <string.h>
#include "router.h"

void * neighbour_list_get_router_id(void * data)
{
    return &((struct neighbour *)data)->router_id;
}

struct neighbour_list * neighbour_list_create()
{
    NEW_STRUCT(ret,neighbour_list);
    ret->array = assoc_array_create(neighbour_list_get_router_id,assoc_array_key_comp_int);
    ret->mutex = mutex_create();
    return ret;
}

void neighbour_list_delete_entry(void * data)
{
    free((struct neighbour *)data);
}

void neighbour_list_destroy(struct neighbour_list * nl)
{
    mutex_destroy(nl->mutex);
    assoc_array_delete_array(nl->array,neighbour_list_delete_entry);
    free(nl);
}

int neighbour_list_process_incoming_hello(struct sr_instance * sr, struct neighbour_list * nl, uint32_t ip, uint32_t rid,
                                           uint32_t aid, uint32_t nmask, uint16_t helloint)
{
    struct neighbour * n;
    int ret = 0;
    mutex_lock(nl->mutex);
    
    n = assoc_array_read(nl->array,&rid);

    if(n == NULL)
    {
        ret = 1;
        Debug("\n\nAdding new Neighbour rid=%d\n\n",rid);
        n = (struct neighbour *)malloc(sizeof(struct neighbour));
        n->router_id = rid;
        n->ip = ip;
        n->aid = aid;
        n->nmask = nmask;
        n->helloint = helloint;
        assoc_array_insert(nl->array,n);
    }
    n->ttl = 3 * helloint;

    mutex_unlock(nl->mutex);
    return ret;
}

int neighbour_list_scan_neighbour(void * data, void * user_data)
{
    struct neighbour * n = (struct neighbour *)data;
    struct fifo * delete_list = (struct fifo *)user_data;
    if(n->ttl <= 0)
    {
        fifo_push(delete_list,n);
    }
    else
    {
        n->ttl -= 1;
    }
    return 0;
}

void neighbour_list_scan_neighbours(struct sr_instance * sr, struct neighbour_list * nl)
{
    struct fifo * delete_list = fifo_create();
    struct neighbour * n;
    int flood = 0;
    mutex_lock(nl->mutex);
    assoc_array_walk_array(nl->array,neighbour_list_scan_neighbour,delete_list);

    while((n = fifo_pop(delete_list)))
    {
        flood = 1;
        assoc_array_delete(nl->array,&n->router_id);
    }
    if(flood)
    {
        link_state_graph_update_forwarding_table(sr);
        interface_list_send_flood(sr);
    }
    mutex_unlock(nl->mutex);
}

struct __neighbour_list_loop_i
{
    void (*fn)(struct neighbour *, void *);
    void * userdata;
};

int neighbour_list_loop_a(void * data, void * userdata)
{
    struct neighbour * n = (struct neighbour *)data;
    struct __neighbour_list_loop_i * nlli = (struct __neighbour_list_loop_i *)userdata;
    nlli->fn(n,nlli->userdata);
    return 0;
}

void neighbour_list_loop(struct neighbour_list * nlist, void (*fn)(struct neighbour *, void *), void * userdata)
{
    mutex_lock(nlist->mutex);
    struct __neighbour_list_loop_i nlli = {fn,userdata};
    assoc_array_walk_array(nlist->array,neighbour_list_loop_a,&nlli);
    mutex_unlock(nlist->mutex);
}

int neighbour_list_empty(struct neighbour_list * nlist)
{
    int ret;
    mutex_lock(nlist->mutex);
    ret = assoc_array_empty(nlist->array);
    mutex_unlock(nlist->mutex);
    return ret;
}
