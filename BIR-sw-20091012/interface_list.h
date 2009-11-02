#ifndef INTERFACE_LIST_H
#define INTERFACE_LIST_H

#include "bi_assoc_array.h"
#include "sr_base_internal.h"
#include "debug.h"
#include "neighbour_list.h"

struct sr_packet;

struct interface_list_entry
{
    struct neighbour_list * n_list;
    struct sr_vns_if * vns_if;
};

struct interface_list
{
    struct bi_assoc_array * array;
    struct sr_instance * sr;
    int time_to_hello;
    int exit_signal;
};

void interface_list_add_interface(struct interface_list * list, struct sr_vns_if * interface);
struct sr_vns_if * interface_list_get_interface_by_ip(struct interface_list * list, uint32_t ip);
int interface_list_get_MAC_and_IP_from_name(struct interface_list * list,
                                            char * interface, uint8_t * MAC, uint32_t * ip);
struct interface_list * interface_list_create(struct sr_instance * sr);
void interface_list_destroy(struct interface_list * list);
int interface_list_ip_exists(struct interface_list * list, uint32_t ip);

void interface_list_show(struct interface_list * list, print_t print);
void interface_list_alert_new_neighbour(struct sr_instance * sr, struct neighbour * n);
void interface_list_alert_neighbour_down(struct sr_instance * sr, struct neighbour * n);
void interface_list_process_incoming_hello(struct sr_packet * packet, struct interface_list * iflist,
                                           char * interface,
                                           uint32_t ip, uint32_t rid, uint32_t aid,
                                           uint32_t nmask, uint16_t helloint);
#endif
