#ifndef INTERFACE_LIST_H
#define INTERFACE_LIST_H

#include "bi_assoc_array.h"
#include "sr_base_internal.h"
#include "debug.h"

struct interface_list
{
    struct bi_assoc_array * array;
};

void interface_list_add_interface(struct interface_list * list, struct sr_vns_if * interface);
struct sr_vns_if * interface_list_get_interface_by_ip(struct interface_list * list, uint32_t ip);
int interface_list_get_MAC_and_IP_from_name(struct interface_list * list,
                                            char * interface, uint8_t * MAC, uint32_t * ip);
struct interface_list * interface_list_create();
void interface_list_destroy(struct interface_list * list);
int interface_list_ip_exists(struct interface_list * list, uint32_t ip);

void interface_list_show(struct interface_list * list, print_t print);

#endif
