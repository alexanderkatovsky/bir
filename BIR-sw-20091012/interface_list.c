#include "interface_list.h"
#include <stdlib.h>
#include <string.h>
#include "router.h"
#include "lwtcp/lwip/sys.h"

int interface_list_send_hello_on_interface(void * data, void * userdata)
{
    struct interface_list_entry * entry = (struct interface_list_entry *)data;
    struct interface_list * iflist = (struct interface_list *)userdata;

    ospf_send_hello(iflist->sr,entry);

    return 0;
}

void interface_list_send_flood(struct sr_instance * sr)
{
    printf("\n\n***Sending Flood***\n\n");
}

void interface_list_send_hello(struct interface_list * iflist)
{
    mutex_lock(iflist->mutex);
    bi_assoc_array_walk_array(iflist->array,interface_list_send_hello_on_interface,iflist);
    mutex_unlock(iflist->mutex);
}

int interface_list_scan_interfaces(void * data, void * user_data)
{
    struct interface_list_entry * entry = (struct interface_list_entry *)data;

    neighbour_list_scan_neighbours((struct sr_instance *)user_data,entry->n_list);
    return 0;
}
    
void interface_list_thread(void * data)
{
    struct interface_list * iflist = (struct interface_list *)data;

    while(1)
    {
        usleep(1000000);

        mutex_lock(iflist->mutex);

        if(iflist->exit_signal == 0)
        {
            iflist->exit_signal = 1;
            return;
        }
        
        bi_assoc_array_walk_array(iflist->array,interface_list_scan_interfaces,iflist->sr);

        if(iflist->time_to_hello <= 0)
        {
            interface_list_send_hello(iflist);
            iflist->time_to_hello = OSPF_DEFAULT_HELLOINT;
        }
        else
        {
            iflist->time_to_hello -= 1;
        }

        if(iflist->time_to_flood <= 0)
        {
            interface_list_send_flood(iflist->sr);
            iflist->time_to_flood = OSPF_DEFAULT_LSUINT;
        }
        else
        {
            iflist->time_to_flood -= 1;
        }

        mutex_unlock(iflist->mutex);
    }
}

void * interface_list_get_IP(void * data)
{
    return &((struct interface_list_entry *)data)->vns_if->ip;
}

void * interface_list_get_name(void * data)
{
    return ((struct interface_list_entry *)data)->vns_if->name;
}


struct interface_list * interface_list_create(struct sr_instance * sr)
{
    NEW_STRUCT(ret,interface_list);
    ret->array = bi_assoc_array_create(interface_list_get_IP,assoc_array_key_comp_int,
                                       interface_list_get_name,assoc_array_key_comp_str);
    ret->exit_signal = 1;
    ret->sr = sr;
    ret->time_to_hello = OSPF_DEFAULT_HELLOINT;
    ret->time_to_flood = OSPF_DEFAULT_LSUINT;
    ret->mutex = mutex_create();

    sys_thread_new(interface_list_thread,ret);
    return ret;
}

void __delete_interface_list(void * data)
{
    neighbour_list_destroy(((struct interface_list_entry *)data)->n_list);
    free(((struct interface_list_entry *)data)->vns_if);
    free((struct interface_list_entry *)data);
}

void interface_list_destroy(struct interface_list * list)
{
    mutex_destroy(list->mutex);
    list->exit_signal = 0;
    while(list->exit_signal == 0)
    {
    }
    bi_assoc_array_delete_array(list->array,__delete_interface_list);
    free(list);
}

void interface_list_add_interface(struct interface_list * list, struct sr_vns_if * interface)
{
    NEW_STRUCT(entry,interface_list_entry);
    NEW_STRUCT(interface_copy,sr_vns_if);
    struct neighbour_list * n_list = neighbour_list_create();
    *interface_copy = *interface;
    entry->vns_if = interface_copy;
    entry->n_list = n_list;
    entry->aid = 0;
    mutex_lock(list->mutex);
    bi_assoc_array_insert(list->array,entry);
    mutex_unlock(list->mutex);    
}

struct sr_vns_if * interface_list_get_interface_by_ip(struct interface_list * list, uint32_t ip)
{
    struct interface_list_entry * entry;
    struct sr_vns_if * ret = NULL;
    mutex_lock(list->mutex);
    entry = bi_assoc_array_read_1(list->array,&ip);

    if(entry)
    {
        ret = entry->vns_if;
    }

    mutex_unlock(list->mutex);
    return ret;
}

int interface_list_get_MAC_and_IP_from_name(struct interface_list * list,
                                            char * interface, uint8_t * MAC, uint32_t * ip)
{
    struct interface_list_entry * entry;
    int ret = 0;
    mutex_lock(list->mutex);
    entry = bi_assoc_array_read_2(list->array,interface);

    struct sr_vns_if * vnsif;
    if(entry)
    {
        vnsif = entry->vns_if;
        if(ip)
        {
            *ip = vnsif->ip;
        }
        if(MAC)
        {
            memcpy(MAC,vnsif->addr,ETHER_ADDR_LEN);
        }
        ret = 1;
    }
    mutex_unlock(list->mutex);
    return ret;
}

int interface_list_ip_exists(struct interface_list * list, uint32_t ip)
{
    mutex_lock(list->mutex);
    int ret = (bi_assoc_array_read_1(list->array,&ip) != NULL);
    mutex_unlock(list->mutex);
    return ret;
}

int interface_list_print_entry(void * data, void * userdata)
{
    struct interface_list_entry * entry = (struct interface_list_entry *)data;
    struct sr_vns_if * vnsif = entry->vns_if;
    print_t print = (print_t)userdata;
    
    print_ip(vnsif->ip,print);
    print("  ");
    print_mac(vnsif->addr,print);
    print("  ");
    print("%s",vnsif->name);
    print("\n");
    return 0;
}


void interface_list_show(struct interface_list * list,print_t print)
{
    mutex_lock(list->mutex);
    print("\nInterface List:\n");
    print("%3s               %3s              %3s\n","ip","MAC","name");
    bi_assoc_array_walk_array(list->array,interface_list_print_entry,print);
    print("\n\n");
    mutex_unlock(list->mutex);
}

void interface_list_process_incoming_hello(struct sr_packet * packet, struct interface_list * iflist,
                                           char * interface,
                                           uint32_t ip, uint32_t rid, uint32_t aid,
                                           uint32_t nmask, uint16_t helloint)
{
    struct interface_list_entry * entry;
    mutex_lock(iflist->mutex);
    entry = bi_assoc_array_read_2(iflist->array, interface);
    if(entry->aid == 0)
    {
        entry->aid = aid;
    }
    if(entry->aid == aid)
    {
        if(entry && (nmask == entry->vns_if->mask))
        {
            neighbour_list_process_incoming_hello(packet->sr, entry->n_list,ip,rid,aid,nmask,helloint);
        }
    }
    mutex_unlock(iflist->mutex);
}

struct __neighbour_loop_i
{
    void (*fn)(struct sr_vns_if *, struct neighbour *, void *);
    void * userdata;
    struct sr_vns_if * vns_if;
};

void interface_list_neighbour_list_loop(struct neighbour * n, void * userdata)
{
    struct __neighbour_loop_i * nli = (struct __neighbour_loop_i *)userdata;
    nli->fn(nli->vns_if,n,nli->userdata);
}

int interface_list_loop_through_neighbours_a(void * data, void * userdata)
{
    struct interface_list_entry * ile = (struct interface_list_entry *)data;
    struct __neighbour_loop_i * nli = (struct __neighbour_loop_i *)userdata;
    nli->vns_if = ile->vns_if;
    neighbour_list_loop(ile->n_list, interface_list_neighbour_list_loop, nli);
    return 0;
}

void interface_list_loop_through_neighbours(struct interface_list * iflist,
                                            void (*fn)(struct sr_vns_if *, struct neighbour *, void *),
                                            void * userdata)
{
    struct __neighbour_loop_i nli = {fn,userdata};
    mutex_lock(iflist->mutex);
    bi_assoc_array_walk_array(iflist->array,interface_list_loop_through_neighbours_a,&nli);
    mutex_unlock(iflist->mutex);
}
