#include "interface_list.h"
#include <stdlib.h>
#include <string.h>
#include "router.h"
#include "lwtcp/lwip/sys.h"
#include "reg_defines.h"

static int output_ports[4] = { 1 << 0, 1 << 2, 1 << 4, 1 << 6 };


static uint32_t interface_list_mac_hi[4] = { ROUTER_OP_LUT_MAC_0_HI,
                                             ROUTER_OP_LUT_MAC_1_HI,
                                             ROUTER_OP_LUT_MAC_2_HI,
                                             ROUTER_OP_LUT_MAC_3_HI };
static uint32_t interface_list_mac_lo[4] = { ROUTER_OP_LUT_MAC_0_LO,
                                             ROUTER_OP_LUT_MAC_1_LO,
                                             ROUTER_OP_LUT_MAC_2_LO,
                                             ROUTER_OP_LUT_MAC_3_LO };

struct interface_list_update_hw_s
{
    int count;
    struct nf2device * device;
};

int interface_list_hw_write_a(void * data, void * userdata)
{
    struct interface_list_entry * ile = (struct interface_list_entry *)data;
    struct interface_list_update_hw_s * iluhs = (struct interface_list_update_hw_s *)userdata;
    uint32_t mac_hi,mac_lo;

    if(iluhs->count >= ROUTER_OP_LUT_DST_IP_FILTER_TABLE_DEPTH)
    {
        return 1;
    }

    writeReg(iluhs->device, ROUTER_OP_LUT_DST_IP_FILTER_TABLE_ENTRY_IP, ntohl(ile->vns_if->ip));
    writeReg(iluhs->device, ROUTER_OP_LUT_DST_IP_FILTER_TABLE_WR_ADDR, ile->i);

    if(iluhs->count < 4)
    {
        mac_lo = ntohl(*((uint32_t *)(ile->vns_if->addr+2)));
        mac_hi = ntohl(*((uint32_t *)(ile->vns_if->addr))) >> 16;
        writeReg(iluhs->device, interface_list_mac_lo[ile->i], mac_lo);
        writeReg(iluhs->device, interface_list_mac_hi[ile->i], mac_hi);
    }

    iluhs->count += 1;
    
    return 0;
}

void interface_list_update_hw(struct sr_instance * sr)
{
#ifdef _CPUMODE_
    struct interface_list_update_hw_s iluhs = {0,&ROUTER(sr)->device};
    int i;
    mutex_lock(INTERFACE_LIST(sr)->mutex);
    bi_assoc_array_walk_array(INTERFACE_LIST(sr)->array, interface_list_hw_write_a, &iluhs);

    writeReg(iluhs.device, ROUTER_OP_LUT_DST_IP_FILTER_TABLE_ENTRY_IP, OSPF_AllSPFRouters);
    writeReg(iluhs.device, ROUTER_OP_LUT_DST_IP_FILTER_TABLE_WR_ADDR, iluhs.count);
    iluhs.count += 1;

    for(i = iluhs.count; i < ROUTER_OP_LUT_DST_IP_FILTER_TABLE_DEPTH; i++)
    {
        writeReg(iluhs.device, ROUTER_OP_LUT_DST_IP_FILTER_TABLE_ENTRY_IP, 0);
        writeReg(iluhs.device, ROUTER_OP_LUT_DST_IP_FILTER_TABLE_WR_ADDR, i);
    }
    mutex_unlock(INTERFACE_LIST(sr)->mutex);    
#endif
}

void interface_list_hw_nat(struct sr_instance * sr, int i, enum e_nat_type type)
{
#ifdef _CPUMODE_
    struct nf2device * device = &ROUTER(sr)->device;

    if(type == E_NAT_TYPE_INBOUND)
    {
        writeReg(device, NAT_ENABLE_IF, 1);
        writeReg(device, NAT_INBOUND_IF, i);
    }
    else if(type == E_NAT_TYPE_OUTBOUND)
    {
        writeReg(device, NAT_ENABLE_IF, 1);
        writeReg(device, NAT_OUTBOUND_IF, i);
    }
#endif
}


int interface_list_send_hello_on_interface(void * data, void * userdata)
{
    struct interface_list_entry * entry = (struct interface_list_entry *)data;
    struct interface_list * iflist = (struct interface_list *)userdata;

    ospf_send_hello(iflist->sr,entry);

    return 0;
}

struct __interface_list_get_lsus_from_neighbours_i
{
    uint32_t aid;
    struct fifo * lsu_list;
};

void __interface_list_get_lsus_from_neighbours_a(struct neighbour * n, void * userdata)
{
    struct __interface_list_get_lsus_from_neighbours_i * ilglfni =
        (struct __interface_list_get_lsus_from_neighbours_i *)userdata;
    struct ospfv2_lsu * lsu;

    if(n->aid == ilglfni->aid)
    {
        lsu = (struct ospfv2_lsu *)malloc(sizeof(struct ospfv2_lsu));
        lsu->subnet = (n->ip) & (n->nmask);
        lsu->mask = n->nmask;
        lsu->rid = n->router_id;
        fifo_push(ilglfni->lsu_list,lsu);
    }
}

struct interface_list_send_flood_i
{
    uint8_t * outbound_lsu_data;
    struct fifo * outbound_lsu_list;
    int n_lsus;
    struct sr_instance * sr;
};

int __interface_list_get_outbound_lsus_a(void * data, void * userdata)
{
    struct interface_list_entry * ile = (struct interface_list_entry *)data;
    struct interface_list_send_flood_i * fi = (struct interface_list_send_flood_i *)userdata;
    struct ospfv2_lsu * lsu;
    struct __interface_list_get_lsus_from_neighbours_i ilglfni = {ile->aid,fi->outbound_lsu_list};

    if(!interface_list_inbound(fi->sr, ile->vns_if->name))
    {
        if(neighbour_list_empty(ile->n_list))
        {
            lsu = (struct ospfv2_lsu *)malloc(sizeof(struct ospfv2_lsu));
            lsu->subnet = (ile->vns_if->ip) & (ile->vns_if->mask);
            lsu->mask = ile->vns_if->mask;
            lsu->rid = 0;
            fifo_push(fi->outbound_lsu_list,lsu);
        }
        else
        {
            neighbour_list_loop(ile->n_list, __interface_list_get_lsus_from_neighbours_a, &ilglfni);
        }
    }

    return 0;
}

struct __interface_list_send_i
{
    struct sr_instance * sr;
    struct sr_vns_if * vns_if;
    uint8_t * data;
    int len;
};

void __interface_list_send_lsu_a(struct neighbour * n, void * userdata)
{
    struct __interface_list_send_i * ilsi = (struct __interface_list_send_i *)userdata;
    struct sr_packet * packet;
    struct sr_vns_if * vns_if = ilsi->vns_if;

    packet = router_construct_packet(ilsi->sr,ilsi->data,ilsi->len,vns_if->name);
    ospf_construct_ospf_header(packet->packet,OSPF_TYPE_LSU,ilsi->len,ROUTER(ilsi->sr)->rid,n->aid);
    ip_construct_ip_header(packet->packet,ilsi->len,0,OSPF_MAX_LSU_TTL,IP_P_OSPF,vns_if->ip,n->ip);
    ip_construct_eth_header(packet->packet,0,0,ETHERTYPE_IP);

    ip_send(packet,n->ip,vns_if->name);

    router_free_packet(packet);
}


uint8_t * __interface_list_lsus_to_data(struct fifo * list)
{
    int n = fifo_length(list);
    struct ospfv2_lsu * lsu;
    struct ospfv2_lsu * data = (struct ospfv2_lsu *)malloc(n * sizeof(struct ospfv2_lsu));
    int i = 0;

    while((lsu = fifo_pop(list)))
    {
        data[i] = *lsu;
        free(lsu);
        i++;
    }

    return (uint8_t*)data;
}

int interface_list_send_flood_a(void * data, void * userdata)
{
    struct interface_list_entry * if_entry = (struct interface_list_entry *)data;
    struct interface_list_send_flood_i * fi = (struct interface_list_send_flood_i *)userdata;
    struct fifo * inbound_lsu_list = fifo_create();
    uint8_t * packet_data;
    uint8_t * inbound_lsu_data;
    struct __interface_list_send_i ilsi;
    struct __interface_list_get_lsus_from_neighbours_i ni = {if_entry->aid,inbound_lsu_list};
    int n_lsus,len;
    
    if(interface_list_inbound(fi->sr, if_entry->vns_if->name))
    {
        neighbour_list_loop(if_entry->n_list, __interface_list_get_lsus_from_neighbours_a, &ni);
    }
    n_lsus = fifo_length(inbound_lsu_list);
    inbound_lsu_data = __interface_list_lsus_to_data(inbound_lsu_list);

    len = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)
        + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr)
        + (n_lsus + fi->n_lsus)*sizeof(struct ospfv2_lsu);

    packet_data = (uint8_t *)malloc(len);
    if(fi->n_lsus > 0)
    {
        memcpy((uint8_t *)B_LSU_START(packet_data), fi->outbound_lsu_data,
               sizeof(struct ospfv2_lsu) * fi->n_lsus);
    }
    if(n_lsus > 0)
    {
        memcpy((uint8_t *)(B_LSU_START(packet_data) + fi->n_lsus),
               inbound_lsu_data, sizeof(struct ospfv2_lsu) * n_lsus);
    }
    free(inbound_lsu_data);    
    fifo_delete(inbound_lsu_list,0);
    
    ospf_construct_lsu_header(packet_data,ROUTER(fi->sr)->ospf_seq, (n_lsus + fi->n_lsus));

    ilsi.sr = fi->sr;
    ilsi.vns_if = if_entry->vns_if;
    ilsi.data = packet_data;
    ilsi.len = len;

    neighbour_list_loop(if_entry->n_list, __interface_list_send_lsu_a, &ilsi);

    free(packet_data);
    return 0;
}

void interface_list_send_flood(struct sr_instance * sr)
{
    struct fifo * outbound_lsu_list = fifo_create();
    struct interface_list_send_flood_i fi;

    ROUTER(sr)->ospf_seq += 1;

    fi.sr = sr;
    fi.outbound_lsu_list = outbound_lsu_list;
    
    mutex_lock(INTERFACE_LIST(sr)->mutex);
    bi_assoc_array_walk_array(INTERFACE_LIST(sr)->array,__interface_list_get_outbound_lsus_a,&fi);

    fi.n_lsus = fifo_length(outbound_lsu_list);
    fi.outbound_lsu_data = __interface_list_lsus_to_data(outbound_lsu_list);
        
    bi_assoc_array_walk_array(INTERFACE_LIST(sr)->array,interface_list_send_flood_a,&fi);
    mutex_unlock(INTERFACE_LIST(sr)->mutex);
    fifo_delete(outbound_lsu_list,0);
    free(fi.outbound_lsu_data);
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
    struct sr_instance * sr = (struct sr_instance *)data;
    struct interface_list * iflist = INTERFACE_LIST(sr);

    while(1)
    {
        if(ROUTER(sr)->ready)
        {
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
        
        usleep(1000000);
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

void interface_list_create(struct sr_instance * sr)
{
    NEW_STRUCT(ret,interface_list);
    ROUTER(sr)->iflist = ret;
    ret->array = bi_assoc_array_create(interface_list_get_IP,assoc_array_key_comp_int,
                                       interface_list_get_name,assoc_array_key_comp_str);
    ret->total = 0;
    ret->exit_signal = 1;
    ret->sr = sr;
    ret->time_to_hello = 0;
    ret->time_to_flood = OSPF_DEFAULT_LSUINT;
    ret->mutex = mutex_create();

    sys_thread_new(interface_list_thread,sr);
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
    struct assoc_array * inbound = OPTIONS(list->sr)->inbound;
    struct assoc_array * outbound = OPTIONS(list->sr)->outbound;
    struct assoc_array * ospf_disabled = OPTIONS(list->sr)->ospf_disabled_interfaces;
    *interface_copy = *interface;
    entry->vns_if = interface_copy;
    entry->n_list = n_list;
    entry->aid = OPTIONS(list->sr)->aid;
    entry->i = list->total;
    entry->port = output_ports[entry->i];
    entry->up = 1;
    list->total += 1;
    entry->nat_type = E_NAT_TYPE_NONE;
    entry->ospf = 1;

    mutex_lock(list->mutex);
    bi_assoc_array_insert(list->array,entry);
    interface_list_update_hw(list->sr);
    mutex_unlock(list->mutex);    
    
    if(inbound && assoc_array_read(inbound, interface->name))
    {
        entry->nat_type = E_NAT_TYPE_INBOUND;
    }
    else if(outbound && assoc_array_read(outbound, interface->name))
    {
        entry->nat_type = E_NAT_TYPE_OUTBOUND;
    }
    
    if(ospf_disabled && assoc_array_read(ospf_disabled, interface->name))
    {
        entry->ospf = 0;
    }

    interface_list_hw_nat(list->sr, entry->i, entry->nat_type);
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

struct sr_vns_if * interface_list_get_interface_by_name(struct interface_list * list, char * name)
{
    struct interface_list_entry * entry;
    struct sr_vns_if * ret = NULL;
    mutex_lock(list->mutex);
    entry = bi_assoc_array_read_2(list->array,name);
    if(entry)
    {
        ret = entry->vns_if;
    }    
    mutex_unlock(list->mutex);

    return ret;
}

void interface_list_set_ip(struct sr_instance * sr, char * name, uint32_t ip, uint32_t subnet)
{
    struct interface_list * list = INTERFACE_LIST(sr);
    struct interface_list_entry * entry;
    mutex_lock(list->mutex);
    if(bi_assoc_array_read_1(list->array, &ip) == NULL)
    {
        entry = bi_assoc_array_delete_2(list->array,name);
        if(entry)
        {
            entry->vns_if->ip = ip;
            entry->vns_if->mask = subnet;
            bi_assoc_array_insert(list->array,entry);
            ospf_send_hello(sr,entry);
        }
    }
    mutex_unlock(list->mutex);
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
    print_ip(vnsif->mask,print);
    print("  ");    
    print_mac(vnsif->addr,print);
    print("  ");
    print("%s    %d     %d      %s\n",vnsif->name,entry->aid, entry->up,
          entry->nat_type == E_NAT_TYPE_INBOUND ? "in"
          : entry->nat_type == E_NAT_TYPE_OUTBOUND ? "out"
          : "no");
    return 0;
}


void interface_list_show(struct interface_list * list,print_t print)
{
    mutex_lock(list->mutex);
    print("\nInterface List:\n");
    print("%3s               %3s               %3s              %3s    %3s    %3s     %3s\n","ip","mask","MAC","name", "aid", "up?", "nat");
    bi_assoc_array_walk_array(list->array,interface_list_print_entry,print);
    print("\n\n");
    mutex_unlock(list->mutex);
}

void interface_list_process_incoming_hello(struct sr_instance * sr, char * interface,
                                           uint32_t ip, uint32_t rid, uint32_t aid,
                                           uint32_t nmask, uint16_t helloint)
{
    struct interface_list * iflist = INTERFACE_LIST(sr);
    struct interface_list_entry * entry;
    mutex_lock(iflist->mutex);
    entry = bi_assoc_array_read_2(iflist->array, interface);
    if(entry)
    {
        if(entry->aid == aid)
        {
            if(neighbour_list_process_incoming_hello(sr, entry->n_list,ip,rid,aid,nmask,helloint))
            {
                ospf_send_hello(sr,entry);
                interface_list_send_flood(sr);
            }
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

struct __interface_list_loop_interfaces_i
{
    void (*fn)(struct sr_vns_if *, void *);
    void * userdata;
};

int __interface_list_loop_interfaces_a(void * data, void * userdata)
{
    struct __interface_list_loop_interfaces_i * lii = (struct __interface_list_loop_interfaces_i *)userdata;
    struct interface_list_entry * ile = (struct interface_list_entry *)data;
    lii->fn(ile->vns_if,lii->userdata);
    return 0;
}

void interface_list_loop_interfaces(struct sr_instance * sr, void (*fn)(struct sr_vns_if *, void *),
                                    void * userdata)
{
    struct __interface_list_loop_interfaces_i lii = {fn,userdata};
    struct interface_list * iflist = INTERFACE_LIST(sr);
    mutex_lock(iflist->mutex);
    bi_assoc_array_walk_array(iflist->array,__interface_list_loop_interfaces_a,&lii);
    mutex_unlock(iflist->mutex);
}


void __interface_list_show_neighbours_a(struct sr_vns_if * vns_if, struct neighbour * n, void * userdata)
{
    print_t print = (print_t)userdata;
    print("interface     :   %s\n", vns_if->name);
    print("router id     :   ");print_ip(n->router_id,print);print("\n");
    print("area id       :   %d\n", n->aid);
    print("ip            :   ");print_ip(n->ip,print);print("\n");
    print("mask          :   ");print_ip(n->nmask,print);print("\n");
    print("hello int     :   %d\n", n->helloint);
    print("ttl           :   %d\n", n->ttl);
    
    print("\n");
}

void interface_list_show_neighbours(struct interface_list * iflist, print_t print)
{
    interface_list_loop_through_neighbours(iflist,__interface_list_show_neighbours_a,print);
}

int interface_list_ip_in_network_on_interface(struct sr_instance * sr, struct ip_address * ip, char * interface)
{
    struct interface_list_entry * entry;
    entry = bi_assoc_array_read_2(INTERFACE_LIST(sr)->array,interface);
    int ret = 0;

    if(entry && (entry->vns_if->mask == ip->mask) &&
       ((entry->vns_if->ip & entry->vns_if->mask) == (ip->subnet & ip->mask)))
    {
        ret = 1;
    }
    return ret;
}

uint32_t interface_list_get_output_port(struct sr_instance * sr, char * interface)
{
    struct interface_list_entry * entry = bi_assoc_array_read_2(INTERFACE_LIST(sr)->array,interface);
    uint32_t ret = 0;
    if(entry != NULL)
    {
        ret = entry->port;
    }
    return ret;
}

struct __interface_list_get_ifname_from_port_i
{
    char * name;
    uint32_t port;
};

int __interface_list_get_ifname_from_port_a(void * data, void * userdata)
{
    struct interface_list_entry * i = (struct interface_list_entry *)data;
    struct __interface_list_get_ifname_from_port_i * pi =
        (struct __interface_list_get_ifname_from_port_i *)userdata;
    int ret = 0;

    if(i->port == pi->port)
    {
        pi->name = i->vns_if->name;
        ret = 1;
    }
    return ret;
}

char * interface_list_get_ifname_from_port(struct sr_instance * sr, uint32_t port)
{
    struct __interface_list_get_ifname_from_port_i i = {0,port};
    bi_assoc_array_walk_array(INTERFACE_LIST(sr)->array,__interface_list_get_ifname_from_port_a, &i);
    return i.name;
}

int interface_list_interface_up(struct sr_instance * sr, char * iface)
{
    struct interface_list_entry * ile = (struct interface_list_entry *)
        bi_assoc_array_read_2(INTERFACE_LIST(sr)->array, iface);
    int ret = 0;
    if(ile && ile->up)
    {
        ret = 1;
    }
    return ret;
}

int interface_list_set_enabled(struct sr_instance * sr, char * iface, int enabled)
{
    struct interface_list_entry * ile = (struct interface_list_entry *)
        bi_assoc_array_read_2(INTERFACE_LIST(sr)->array, iface);
    int ret = -1;
    if(ile)
    {
        if(ile->up == enabled)
        {
            ret = 1;
        }
        else
        {
            ile->up = enabled;
            ret = 0;
        }
    }
    return ret;
}

int interface_list_nat_type(struct sr_instance * sr, char * name, enum e_nat_type type)
{
    if(name == NULL) return 0;
    struct interface_list_entry * e;
    mutex_lock(INTERFACE_LIST(sr)->mutex);
    e = bi_assoc_array_read_2(INTERFACE_LIST(sr)->array, name);
    mutex_unlock(INTERFACE_LIST(sr)->mutex);
    if(e)
    {
        return e->nat_type == type;
    }
    return 0;    
}

int interface_list_inbound(struct sr_instance * sr, char * name)
{
    return interface_list_nat_type(sr, name, E_NAT_TYPE_INBOUND);
}

int interface_list_outbound(struct sr_instance * sr, char * name)
{
    return interface_list_nat_type(sr, name, E_NAT_TYPE_OUTBOUND);
}

int interface_list_nat_enabled(struct sr_instance * sr, char * name)
{
    return !interface_list_nat_type(sr, name, E_NAT_TYPE_NONE);
}

int interface_list_forward_packet(struct sr_instance * sr, char * from, char * to)
{
    int ret = 
        (!interface_list_inbound(sr,from) && !interface_list_inbound(sr,to)) ||
        strcmp(from,to) == 0;
    return ret;
}

int interface_list_forward_lsu(struct sr_instance * sr, char * from, char * to)
{
    return
        (!interface_list_inbound(sr,from) || strcmp(from,to) == 0) &&
        interface_list_ospf_enabled(sr, from) &&
        interface_list_ospf_enabled(sr, to);
}

int interface_list_ospf_enabled(struct sr_instance * sr, char * interface)
{
    struct interface_list_entry * entry = bi_assoc_array_read_2(INTERFACE_LIST(sr)->array, interface);
    if(entry)
    {
        return !OPTIONS(sr)->disable_ospf && entry->ospf;
    }
    return 0;
}
