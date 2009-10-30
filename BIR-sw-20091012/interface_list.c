#include "interface_list.h"
#include <stdlib.h>
#include <string.h>
#include "eth_headers.h"

void * interface_list_key_get(void * data)
{
    return &((struct sr_vns_if *)data)->ip;
}


struct interface_list * interface_list_create()
{
    struct interface_list * ret = (struct interface_list *)malloc(sizeof(struct interface_list));
    ret->array = assoc_array_create(interface_list_key_get,assoc_array_key_comp_int);
    return ret;
}

void __delete_interface_list(void * data)
{
    free((struct sr_vns_if *)data);
}

void interface_list_destroy(struct interface_list * list)
{
    assoc_array_delete_array(list->array,__delete_interface_list);
    free(list);
}

void interface_list_add_interface(struct interface_list * list, struct sr_vns_if * interface)
{
    struct sr_vns_if * interface_copy = (struct sr_vns_if *)malloc(sizeof(struct sr_vns_if));
    *interface_copy = *interface;
    assoc_array_insert(list->array,interface_copy);
}

struct sr_vns_if * interface_list_get_interface_by_ip(struct interface_list * list, uint32_t ip)
{
    return (struct sr_vns_if *) assoc_array_read(list->array,&ip);
}

struct __search_by_name_s
{
    struct sr_vns_if * vnsif;
    const char * interface;
};

int __search_by_name(void * data, void * user_data)
{
    struct __search_by_name_s * s = (struct __search_by_name_s *) user_data;
    struct sr_vns_if * vnsif = (struct sr_vns_if *) data;
    if(strcmp(vnsif->name,s->interface) == 0)
    {
        s->vnsif = vnsif;
        return 1;
    }
    return 0;
}

int interface_list_get_MAC_and_IP_from_name(struct interface_list * list, const char * interface, uint8_t * MAC, uint32_t * ip)
{
    struct __search_by_name_s s = {0,interface};
    assoc_array_walk_array(list->array,__search_by_name,&s);
    if(s.vnsif)
    {
        if(ip)
        {
            *ip = s.vnsif->ip;
        }
        if(MAC)
        {
            memcpy(MAC,s.vnsif->addr,ETHER_ADDR_LEN);
        }
        return 1;
    }
    return 0;
}

int interface_list_ip_exists(struct interface_list * list, uint32_t ip)
{
    return (assoc_array_read(list->array,&ip) != NULL);
}
