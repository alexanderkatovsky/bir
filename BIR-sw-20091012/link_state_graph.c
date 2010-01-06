#include "link_state_graph.h"
#include "common.h"
#include "router.h"


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

int __link_cmp(void * a, void * b)
{
    struct link * l1 = (struct link *)a;
    struct link * l2 = (struct link *)b;
    int ret = ip_address_cmp(&l1->ip, &l2->ip);
    if(ret == ASSOC_ARRAY_KEY_EQ)
    {
        if(l1->rid < l2->rid) return ASSOC_ARRAY_KEY_LT;
        if(l1->rid > l2->rid) return ASSOC_ARRAY_KEY_GT;
        return ASSOC_ARRAY_KEY_EQ;
    }
    return ret;
}

void link_state_graph_free_node(void * data)
{
    struct link_state_node * lsn = (struct link_state_node *)data;
    assoc_array_delete_array(lsn->links,assoc_array_delete_self);
    free(lsn);
}


void __link_state_graph_delete_node(struct sr_instance * sr, uint32_t rid)
{
    struct link_state_node * lsn = assoc_array_delete(LSG(sr)->array, &rid);
    if(lsn)
    {
        link_state_graph_free_node(lsn);
    }
}

struct link_state_node * __link_state_graph_insert_node(struct link_state_graph * lsg, uint32_t rid)
{
    struct link_state_node * lsn;
    lsn = (struct link_state_node *)malloc(sizeof(struct link_state_node));
    lsn->rid = rid;
    lsn->seq = -1;
    lsn->links = assoc_array_create(assoc_array_get_self,__link_cmp);
    assoc_array_insert(lsg->array,lsn);
    return lsn;
}

int __link_state_graph_link_cmp(void * d1, void * d2)
{
    struct link * l1 = (struct link *)d1;
    struct link * l2 = (struct link *)d2;

    return
        l1->ip.subnet == l2->ip.subnet &&
        l1->ip.mask   == l2->ip.mask   &&
        l1->rid       == l2->rid;
}

/* called by link_state_graph_update_links ->  ospf_handle_incoming_lsu; updates the
 * link state graph for a node and returns 1 if the forwarding table needs updating
 * */
int link_state_graph_update_links_list(struct sr_instance * sr,
                                       struct assoc_array ** links, uint32_t num, struct ospfv2_lsu * adv)
{
    uint32_t i;
    struct assoc_array * links2;
    struct link * lk;
    int ret = 0;

    links2 = assoc_array_create(assoc_array_get_self,__link_cmp);

    for(i = 0; i < num; i++)
    {
        lk = (struct link *)malloc(sizeof(struct link));
        lk->ip.subnet = adv[i].subnet;
        lk->ip.mask = adv[i].mask;
        lk->rid = adv[i].rid;
        if(assoc_array_read(links2,lk) == NULL)
        {
            assoc_array_insert(links2,lk);
        }
        else
        {
            free(lk);
        }
    }

    if(!assoc_array_eq(*links, links2, __link_state_graph_link_cmp))
    {
        ret = 1;
    }
    assoc_array_delete_array(*links,assoc_array_delete_self);
    *links = links2;

    return ret;
}

struct dijkstra_i
{
    struct linked_list * dl;
    struct linked_list * dijkstra_list;
    struct assoc_array * visited;
    struct sr_instance * sr;
    struct dijkstra * d;
    int nat;
};

void link_state_graph_dijkstra_initial(struct sr_vns_if * vns_if, struct neighbour * n, void * userdata)
{
    struct dijkstra_i * di = (struct dijkstra_i *)userdata;
    struct ip_address address = {n->ip, n->nmask};
    struct dijkstra * d;
    int outbound = interface_list_outbound(di->sr, vns_if->name);

    if(!di->nat || outbound)
    {
        d = (struct dijkstra *)malloc(sizeof(struct dijkstra));
        d->address = address;
        d->rid = n->router_id;
        d->next_hop = n->ip;
        strcpy(d->interface, vns_if->name);
    
        linked_list_add(di->dijkstra_list, d);
    }
}

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

    if(!forwarding_table_dynamic_entry_exists(FORWARDING_TABLE(di->sr),&d->address, di->nat))
    {
        forwarding_table_add(di->sr,&d->address,d->next_hop,d->interface,1, di->nat);
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
    struct dijkstra_i * di = (struct dijkstra_i *)userdata;
    struct ip_address ip = {iface->ip & iface->mask, iface->mask};
    int outbound = interface_list_outbound(di->sr, iface->name);

    if(!di->nat || outbound)
    {
        if(!forwarding_table_dynamic_entry_exists(FORWARDING_TABLE(di->sr),&ip,di->nat))
        {
            forwarding_table_add(di->sr, &ip, 0, iface->name,1,di->nat);
        }
    }
}

struct __link_state_graph_deprecate_non_visited_nodes_i
{
    struct assoc_array * visited;
    struct fifo * delete;
};

int __link_state_graph_deprecate_non_visited_nodes_a(void * data, void * userdata)
{
    struct __link_state_graph_deprecate_non_visited_nodes_i * vni =
        (struct __link_state_graph_deprecate_non_visited_nodes_i *)userdata;
    struct link_state_node * lsn = (struct link_state_node *)data;

    if(assoc_array_read(vni->visited, &lsn->rid) == NULL)
    {
        fifo_push(vni->delete,lsn);
    }
    return 0;
}

void link_state_graph_update_forwarding_table(struct sr_instance * sr)
{
    /*[(ip-address (subnet,mask), rid, interface, next-hop)]*/
    struct dijkstra_i di;
    struct __link_state_graph_deprecate_non_visited_nodes_i vni;
    struct link_state_node * lsn;

    mutex_lock(LSG(sr)->mutex);

    /*clear dynamic entries and lock mutex*/
    forwarding_table_start_dijkstra(sr);
    
    di.sr = sr;

    for(di.nat = router_nat_enabled(sr); di.nat >= 0; di.nat--)
    {
        di.visited = assoc_array_create(link_state_graph_visited_get_key, assoc_array_key_comp_int);
        di.dijkstra_list = linked_list_create();
        interface_list_loop_through_neighbours(INTERFACE_LIST(sr), link_state_graph_dijkstra_initial, &di);
        while(!linked_list_empty(di.dijkstra_list))
        {
            di.dl = linked_list_create();
            linked_list_walk_list(di.dijkstra_list, link_state_graph_dijkstra_next, &di);
            linked_list_delete_list(di.dijkstra_list, link_state_graph_delete_dijkstra_list);
            di.dijkstra_list = di.dl;
        }
        interface_list_loop_interfaces(sr, __link_state_graph_add_if_routes_a, &di);
        linked_list_delete_list(di.dijkstra_list,link_state_graph_delete_dijkstra_list);
        if(di.nat)
        {
            assoc_array_delete_array(di.visited, link_state_graph_delete_vistited);
        }
    }

    vni.visited = di.visited;
    vni.delete  = fifo_create();

    assoc_array_walk_array(LSG(sr)->array, __link_state_graph_deprecate_non_visited_nodes_a, &vni);

    while((lsn = fifo_pop(vni.delete)))
    {
        __link_state_graph_delete_node(sr, lsn->rid);
    }

    fifo_delete(vni.delete,0);
    assoc_array_delete_array(di.visited, link_state_graph_delete_vistited);
      
    /*unlock mutex*/
    forwarding_table_end_dijkstra(sr);
    mutex_unlock(LSG(sr)->mutex);
}

int link_state_graph_update_links(struct sr_instance * sr,
                               uint32_t rid, uint16_t seq, uint32_t num, struct ospfv2_lsu * adv)
{
    struct link_state_node * lsn;
    struct link_state_graph * lsg = LSG(sr);
    int ret = 1;

    mutex_lock(lsg->mutex);

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
        if(link_state_graph_update_links_list(sr, &lsn->links, num, adv))
        {
            link_state_graph_update_forwarding_table(sr);
        }
    }

    mutex_unlock(lsg->mutex);

    return ret;
}

void * link_state_graph_get_key(void * data)
{
    return &((struct link_state_node *)data)->rid;
}

void link_state_graph_destroy(struct link_state_graph * lsg)
{
    assoc_array_delete_array(lsg->array,link_state_graph_free_node);
    free(lsg);
    mutex_destroy(lsg->mutex);
}

void link_state_graph_create(struct sr_instance * sr)
{
    NEW_STRUCT(ret,link_state_graph);
    ROUTER(sr)->lsg = ret;
    ret->array = assoc_array_create(link_state_graph_get_key,assoc_array_key_comp_int);
    ret->mutex = mutex_create();
}

int __link_state_graph_show_link_a(void * data, void * userdata)
{
    struct link * l = (struct link *)data;
    print_t print = (print_t)userdata;
    print("[");
    print(" ip:    ");print_ip(l->ip.subnet,print);
    print(" mask:  ");print_ip(l->ip.mask,print);
    print(" rid:   ");print_ip(l->rid,print);
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

struct __loop_links_i
{
    void (*fn)(uint32_t rid_node, int seq, uint32_t rid_link, struct ip_address, void *);
    struct link_state_node * node;
    void * userdata;
};

static int __loop_links_a(void * data, void * userdata)
{
    struct link * lk = (struct link *)data;
    struct __loop_links_i * i = (struct __loop_links_i *)userdata;
    i->fn(i->node->rid, i->node->seq, lk->rid, lk->ip, i->userdata);
    return 0;
}

static int __loop_nodes_a(void * data, void * userdata)
{
    struct link_state_node * node = (struct link_state_node *)data;
    struct __loop_links_i * i = (struct __loop_links_i *)userdata;
    i->node = node;
    assoc_array_walk_array(node->links, __loop_links_a, i);
    return 0;
}

void link_state_graph_loop_links(struct sr_instance * sr, void (*fn)(uint32_t rid_node, int seq,
                                                                     uint32_t rid_link,
                                                                     struct ip_address, void *), void * userdata)
{
    struct __loop_links_i i = {fn, 0, userdata};
    assoc_array_walk_array(LSG(sr)->array, __loop_nodes_a, &i);
}
