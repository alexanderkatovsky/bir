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

void * __links_getter(void * data)
{
    return &((struct link *)data)->ip;
}

int link_state_graph_update_links_list(struct assoc_array ** links, uint32_t num, struct ospfv2_lsu * adv)
{
    /*could check if there has been a change*/
    uint32_t i;
    int ret = 1;
    struct link * lk;
    assoc_array_delete_array(*links,link_state_graph_links_list_delete_link);
    *links = assoc_array_create(__links_getter,ip_address_cmp);

    for(i = 0; i < num; i++)
    {
        lk = (struct link *)malloc(sizeof(struct link));
        lk->ip.subnet = adv[i].subnet;
        lk->ip.mask = adv[i].mask;
        lk->rid = adv[i].rid;
        assoc_array_insert(*links,lk);
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
        forwarding_table_add(di->sr,&d->address,d->next_hop,d->interface,1);
    }
    if((lsn != NULL) && (assoc_array_read(di->visited,&d->rid) == NULL))
    {
        v = (uint32_t *)malloc(sizeof(uint32_t));
        *v = d->rid;
        assoc_array_insert(di->visited, v);
        di->d = d;

        assoc_array_walk_array(lsn->links,link_state_graph_insert_links,di);
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

void __link_state_graph_add_if_routes_a(struct sr_vns_if * iface, void * userdata)
{
    struct sr_instance * sr = (struct sr_instance *)userdata;
    struct ip_address ip = {iface->ip & iface->mask, iface->mask};
    forwarding_table_add(sr, &ip, 0, iface->name,1);
}

void link_state_graph_update_forwarding_table(struct sr_instance * sr)
{

    /*[(ip-address (subnet,mask), rid, interface, next-hop)]*/
    struct linked_list * dijkstra_list = linked_list_create();
    struct dijkstra_i di;
    
    Debug("\n\n***Updating forwarding table... ");

    /*clear dynamic entries and lock mutex*/
    forwarding_table_start_dijkstra(sr);
    interface_list_loop_interfaces(sr, __link_state_graph_add_if_routes_a, sr);
    
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
    forwarding_table_end_dijkstra(sr);

    Debug("... Finished Updating forwarding table***\n\n");

/*    forwarding_table_dynamic_show(FORWARDING_TABLE(sr),printf);*/
}

struct link_state_node * __link_state_graph_insert_node(struct link_state_graph * lsg, uint32_t rid)
{
    struct link_state_node * lsn;
    lsn = (struct link_state_node *)malloc(sizeof(struct link_state_node));
    lsn->rid = rid;
    lsn->seq = -1;
    lsn->links = assoc_array_create(__links_getter,ip_address_cmp);
    lsn->n_neighbours = 0;
    assoc_array_insert(lsg->array,lsn);
    return lsn;
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
        lsn = __link_state_graph_insert_node(lsg, rid);
    }
    else if(lsn->seq >= seq)
    {
        ret = 0;
    }
    
    if(ret)
    {
        lsn->seq = seq;
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
    assoc_array_delete_array(lsn->links,link_state_graph_links_list_delete_link);
    free(lsn);
}

void * link_state_graph_get_key(void * data)
{
    return &((struct link_state_node *)data)->rid;
}

void link_state_graph_neighbour_up(struct sr_instance * sr, uint32_t rid, uint32_t ip)
{
    struct link_state_node * node = (struct link_state_node *) assoc_array_delete(LSG(sr)->array, &rid);
    if(node == NULL)
    {
        node = __link_state_graph_insert_node(LSG(sr), rid);
    }
    node->n_neighbours += 1;
}

void link_state_graph_neighbour_down(struct sr_instance * sr, uint32_t rid, uint32_t ip)
{
    struct link_state_node * node = (struct link_state_node *) assoc_array_delete(LSG(sr)->array, &rid);
    if(node)
    {
        node->n_neighbours -= 1;
        if(node->n_neighbours <= 0)
        {
            link_state_graph_delete_node(node);
        }
    }
}

void link_state_graph_destroy(struct link_state_graph * lsg)
{
    assoc_array_delete_array(lsg->array,link_state_graph_delete_node);
}

void link_state_graph_create(struct sr_instance * sr)
{
    NEW_STRUCT(ret,link_state_graph);
    ROUTER(sr)->lsg = ret;
    ret->array = assoc_array_create(link_state_graph_get_key,assoc_array_key_comp_int);
}

int __link_state_graph_show_link_a(void * data, void * userdata)
{
    struct link * l = (struct link *)data;
    print_t print = (print_t)userdata;
    print("[");
    print("ip:   ");print_ip(l->ip.subnet,print);
    print("mask: ");print_ip(l->ip.mask,print);
    print("rid:  ");print_ip(l->rid,print);
    print("]\n");

    return 0;
}

int __link_state_graph_show_topology_a(void * data, void * userdata)
{
    struct link_state_node * lsn = (struct link_state_node *)data;
    print_t print = (print_t)userdata;
    print("  rid:  ");print_ip(lsn->rid,print);print("\n");
    print("  last sequence number: %d\n", lsn->seq);

    assoc_array_walk_array(lsn->links,__link_state_graph_show_link_a,print);
    print("\n");
    
    return 0;
}

void link_state_graph_show_topology(struct link_state_graph * lsg, print_t print)
{
    assoc_array_walk_array(lsg->array,__link_state_graph_show_topology_a,print);
}
