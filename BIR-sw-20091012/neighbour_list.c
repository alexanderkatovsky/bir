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

void neighbour_list_process_incoming_hello(struct sr_instance * sr, struct neighbour_list * nl, uint32_t ip, uint32_t rid,
                                           uint32_t aid, uint32_t nmask, uint16_t helloint)
{
    struct neighbour * n;
    mutex_lock(nl->mutex);
    
    n = assoc_array_read(nl->array,&rid);

    if(n == NULL)
    {
        n = (struct neighbour *)malloc(sizeof(struct neighbour));
        n->router_id = rid;
        n->ip = ip;
        n->aid = aid;
        n->nmask = nmask;
        n->helloint = helloint;
        assoc_array_insert(nl->array,n);
        interface_list_alert_new_neighbour(sr, n);
    }
    n->ttl = 3 * helloint;

    mutex_unlock(nl->mutex);
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
    mutex_lock(nl->mutex);
    assoc_array_walk_array(nl->array,neighbour_list_scan_neighbour,delete_list);

    while((n = fifo_pop(delete_list)))
    {
        assoc_array_delete(nl->array,&n->router_id);
        interface_list_alert_neighbour_down(sr, n);
    }
    mutex_unlock(nl->mutex);
}

