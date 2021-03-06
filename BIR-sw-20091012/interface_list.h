#ifndef INTERFACE_LIST_H
#define INTERFACE_LIST_H

#include "bi_assoc_array.h"
#include "sr_base_internal.h"
#include "debug.h"
#include "neighbour_list.h"
#include "common.h"

struct sr_packet;


enum e_nat_type
{
    E_NAT_TYPE_INBOUND,
    E_NAT_TYPE_OUTBOUND,
    E_NAT_TYPE_NONE
};

struct interface_list_entry
{
    struct neighbour_list * n_list;
    struct sr_vns_if * vns_if;
    uint32_t aid;
    int i;
    uint32_t port;
    int ospf;

    enum e_nat_type nat_type;

    int up;
};

struct interface_list
{
    struct bi_assoc_array * array;
    struct sr_instance * sr;
    int time_to_hello;
    int time_to_flood;
    int total;

    struct sr_mutex * mutex;
};

void interface_list_add_interface(struct interface_list * list, struct sr_vns_if * interface);
struct sr_vns_if * interface_list_get_interface_by_ip(struct interface_list * list, uint32_t ip);
struct sr_vns_if * interface_list_get_interface_by_name(struct interface_list * list, char * name);
int interface_list_get_MAC_and_IP_from_name(struct interface_list * list,
                                            char * interface, uint8_t * MAC, uint32_t * ip);
void interface_list_get_params(struct sr_instance * sr, char * name, uint32_t * aid, int * ospf,
                               enum e_nat_type * nat_type, int * up);
void interface_list_set_ip(struct sr_instance * sr, char * name, uint32_t ip, uint32_t subnet);
void interface_list_set_aid(struct sr_instance * sr, char * name, uint32_t aid);
void interface_list_set_mac(struct sr_instance * sr, char * name, uint8_t * mac);
void interface_list_set_params(struct sr_instance * sr, char * name, uint32_t aid, int  ospf,
                               enum e_nat_type nat_type, int up);
void interface_list_create(struct sr_instance * sr);
void interface_list_destroy(struct interface_list * list);
int interface_list_ip_exists(struct interface_list * list, uint32_t ip);

void interface_list_show(struct interface_list * list, print_t print);
void interface_list_process_incoming_hello(struct sr_instance * sr, char * interface,
                                           uint32_t ip, uint32_t rid, uint32_t aid,
                                           uint32_t nmask, uint16_t helloint);

void interface_list_loop_through_neighbours(struct interface_list * iflist,
                                            void (*fn)(struct sr_vns_if *, struct neighbour *, void *),
                                            void * userdata);
void interface_list_loop_interfaces(struct sr_instance * sr, void (*fn)(struct sr_vns_if *, void *),
                                    void * userdata);

void interface_list_send_flood(struct sr_instance * sr);
void interface_list_show_neighbours(struct interface_list * iflist, print_t print);
int interface_list_ip_in_network_on_interface(struct sr_instance * sr, struct ip_address * ip, char * interface);
uint32_t interface_list_get_output_port(struct sr_instance * sr, char * interface);
uint32_t interface_list_get_nat_output_port(struct sr_instance * sr, char * interface);
char * interface_list_get_ifname_from_port(struct sr_instance * sr, uint32_t port);
char * interface_list_get_ifname_from_nat_port(struct sr_instance * sr, uint32_t port);

int interface_list_interface_up(struct sr_instance * sr, char * iface);
int interface_list_set_enabled(struct sr_instance * sr, char * iface, int enabled);

int interface_list_inbound(struct sr_instance * sr, char * name);
int interface_list_outbound(struct sr_instance * sr, char * name);
int interface_list_nat_enabled(struct sr_instance * sr, char * name);

int interface_list_forward_packet(struct sr_instance * sr, char * from, char * to);
int interface_list_forward_lsu(struct sr_instance * sr, char * from, char * to, uint32_t from_ip);
int interface_list_ospf_enabled(struct sr_instance * sr, char * interface);

void interface_list_get_ospf_info(struct sr_instance * sr, int * hello, int * flood);

#endif
