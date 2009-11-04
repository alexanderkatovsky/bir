#include "link_state_graph.h"
#include "common.h"
#include "router.h"

/*
 * rid --> [(subnet, mask, rid),...]
 * rid --> [(subnet, mask, rid),...]
 *   .
 *   .
 *   .
 * */

void * link_state_graph_links_array_get_key(void * data)
{
    return data;
}

int ip_address_cmp(void * k1, void * k2)
{
    struct ip_address * ki1 = (struct ip_address *)k1;
    struct ip_address * ki2 = (struct ip_address *)k2;

    if(ki1->subnet < ki2->subnet) return ASSOC_ARRAY_KEY_LT;
    else if(ki1->subnet > ki2->subnet) return ASSOC_ARRAY_KEY_GT;
    else
    {
        if(ki1->mask < ki2->mask) return ASSOC_ARRAY_KEY_LT;
        else if(ki1->mask > ki2->mask) return ASSOC_ARRAY_KEY_GT;
        else return ASSOC_ARRAY_KEY_EQ;
    }
}

void link_state_graph_links_list_delete_link(void * data)
{
    free((struct link *)data);
}

int link_state_graph_update_links_list(struct linked_list ** links, uint32_t num, struct ospfv2_lsu * adv)
{
    /*could check if there has been a change*/
    uint32_t i;
    int ret = 1;
    struct link * lk;
    linked_list_delete_list(*links,link_state_graph_links_list_delete_link);
    *links = linked_list_create();

    for(i = 0; i < num; i++)
    {
        lk = (struct link *)malloc(sizeof(struct link));
        lk->ip.subnet = adv[i].subnet;
        lk->ip.mask = adv[i].mask;
        lk->rid = adv[i].rid;
        linked_list_add(*links,lk);
    }

    return ret;
}

void link_state_graph_dijkstra_initial(struct sr_vns_if * vns_if, struct neighbour * n, void * userdata)
{
    struct linked_list * dl = (struct linked_list *)userdata;
    struct ip_address address = {n->ip, n->nmask};
    struct dijkstra * d;

    d = (struct dijkstra *)malloc(sizeof(struct dijkstra));
    d->address = address;
    d->rid = n->router_id;
    d->next_hop = n->ip;
    strcpy(d->interface, vns_if->name);
    
    linked_list_add(dl, d);
}

struct dijkstra_i
{
    struct linked_list * dl;
    struct assoc_array * visited;
    struct sr_instance * sr;
    struct dijkstra * d;
};

int link_state_graph_insert_links(void * data, void * userdata)
{
    struct link * l = (struct link *)data;
    struct dijkstra_i * di = (struct dijkstra_i *)userdata;
    NEW_STRUCT(d,dijkstra);
    *d = *di->d;
    d->address = l->ip;
    d->rid = l->rid;
    linked_list_add(di->dl,d);

    return 0;
}

int link_state_graph_dijkstra_next(void * data, void * userdata)
{
    struct dijkstra * d = (struct dijkstra *)data;
    struct dijkstra_i * di = (struct dijkstra_i *)userdata;
    uint32_t * v;
    struct link_state_node * lsn = (struct link_state_node *)assoc_array_read(LSG(di->sr)->array,&d->rid);
    if(!forwarding_table_dynamic_entry_exists(FORWARDING_TABLE(di->sr),&d->address))
    {
        forwarding_table_add_dynamic(FORWARDING_TABLE(di->sr),&d->address,d->next_hop,d->interface);
    }
    if((lsn != NULL) && (assoc_array_read(di->visited,&d->rid) == NULL))
    {
        v = (uint32_t *)malloc(sizeof(uint32_t));
        *v = d->rid;
        assoc_array_insert(di->visited, v);
        di->d = d;

        linked_list_walk_list(lsn->links,link_state_graph_insert_links,di);
    }
    return 0;
}

void link_state_graph_delete_dijkstra_list(void * data)
{
    free((struct dijkstra *)data);
}

void * link_state_graph_visited_get_key(void * data)
{
    return data;
}

void link_state_graph_delete_vistited(void * data)
{
    free((uint32_t *)data);
}

void link_state_graph_update_forwarding_table(struct sr_instance * sr)
{

    /*[(ip-address (subnet,mask), rid, interface, next-hop)]*/
    struct linked_list * dijkstra_list = linked_list_create();
    struct dijkstra_i di;
    
    Debug("\n\n***Updating forwarding table... ");

    /*clear dynamic entries and lock mutex*/
    forwarding_table_start_dijkstra(FORWARDING_TABLE(sr));
    
    di.sr = sr;
    di.visited = assoc_array_create(link_state_graph_visited_get_key, assoc_array_key_comp_int);

    interface_list_loop_through_neighbours(INTERFACE_LIST(sr), link_state_graph_dijkstra_initial, dijkstra_list);

    while(!linked_list_empty(dijkstra_list))
    {
        di.dl = linked_list_create();
        linked_list_walk_list(dijkstra_list, link_state_graph_dijkstra_next, &di);
        linked_list_delete_list(dijkstra_list, link_state_graph_delete_dijkstra_list);
        dijkstra_list = di.dl;
    }

    linked_list_delete_list(dijkstra_list,link_state_graph_delete_dijkstra_list);
    assoc_array_delete_array(di.visited, link_state_graph_delete_vistited);
        
    /*unlock mutex*/
    forwarding_table_end_dijkstra(FORWARDING_TABLE(sr));

    Debug("... Finished Updating forwarding table***\n\n");

    forwarding_table_dynamic_show(FORWARDING_TABLE(sr),printf);
}

int link_state_graph_update_links(struct sr_instance * sr,
                               uint32_t rid, uint16_t seq, uint32_t num, struct ospfv2_lsu * adv)
{
    struct link_state_node * lsn;
    struct link_state_graph * lsg = LSG(sr);
    int ret = 1;
    lsn = (struct link_state_node *)assoc_array_read(lsg->array,&rid);
    if(lsn == NULL)
    {
        lsn = (struct link_state_node *)malloc(sizeof(struct link_state_node));
        lsn->rid = rid;
        lsn->seq = seq;
        lsn->links = linked_list_create();
        assoc_array_insert(lsg->array,lsn);
    }
    else if(lsn->seq >= seq)
    {
        ret = 0;
    }
    
    if(ret)
    {
        if(link_state_graph_update_links_list(&lsn->links,num,adv))
        {
            link_state_graph_update_forwarding_table(sr);
        }
    }

    return ret;
}

void link_state_graph_delete_node(void * data)
{
    struct link_state_node * lsn = (struct link_state_node *)data;
    linked_list_delete_list(lsn->links,link_state_graph_links_list_delete_link);
    free(lsn);
}

void * link_state_graph_get_key(void * data)
{
    return &((struct link_state_node *)data)->rid;
}

void link_state_graph_destroy(struct link_state_graph * lsg)
{
    assoc_array_delete_array(lsg->array,link_state_graph_delete_node);
}

struct link_state_graph * link_state_graph_create()
{
    NEW_STRUCT(ret,link_state_graph);
    ret->array = assoc_array_create(link_state_graph_get_key,assoc_array_key_comp_int);
    return ret;
}


