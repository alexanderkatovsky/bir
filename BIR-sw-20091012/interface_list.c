#include "interface_list.h"
#include <stdlib.h>
#include <string.h>
#include "eth_headers.h"

void * interface_list_get_IP(void * data)
{
    return &((struct sr_vns_if *)data)->ip;
}

void * interface_list_get_name(void * data)
{
    return ((struct sr_vns_if *)data)->name;
}


struct interface_list * interface_list_create()
{
    struct interface_list * ret = (struct interface_list *)malloc(sizeof(struct interface_list));
    ret->array = bi_assoc_array_create(interface_list_get_IP,assoc_array_key_comp_int,
                                       interface_list_get_name,assoc_array_key_comp_str);
    return ret;
}

void __delete_interface_list(void * data)
{
    free((struct sr_vns_if *)data);
}

void interface_list_destroy(struct interface_list * list)
{
    bi_assoc_array_delete_array(list->array,__delete_interface_list);
    free(list);
}

void interface_list_add_interface(struct interface_list * list, struct sr_vns_if * interface)
{
    struct sr_vns_if * interface_copy = (struct sr_vns_if *)malloc(sizeof(struct sr_vns_if));
    *interface_copy = *interface;
    bi_assoc_array_insert(list->array,interface_copy);
}

struct sr_vns_if * interface_list_get_interface_by_ip(struct interface_list * list, uint32_t ip)
{
    return (struct sr_vns_if *) bi_assoc_array_read_1(list->array,&ip);
}

int interface_list_get_MAC_and_IP_from_name(struct interface_list * list,
                                            char * interface, uint8_t * MAC, uint32_t * ip)
{
    struct sr_vns_if * vnsif = bi_assoc_array_read_2(list->array,interface);
    if(vnsif)
    {
        if(ip)
        {
            *ip = vnsif->ip;
        }
        if(MAC)
        {
            memcpy(MAC,vnsif->addr,ETHER_ADDR_LEN);
        }
        return 1;
    }
    return 0;
}

int interface_list_ip_exists(struct interface_list * list, uint32_t ip)
{
    return (bi_assoc_array_read_1(list->array,&ip) != NULL);
}
