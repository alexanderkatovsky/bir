#include "router.h"
#include <string.h>
#include <stdlib.h>
#include "sr_ifsys.h"
#include "nf2.h"

struct sr_packet * router_construct_packet(struct sr_instance * sr,
                                           const uint8_t * packet, unsigned int len, const char* interface)
{
    struct sr_packet * ret = (struct sr_packet *)malloc(sizeof(struct sr_packet));

    ret->sr = sr;
    ret->packet = (uint8_t *) malloc(len*sizeof(uint8_t));
    memcpy(ret->packet,packet,len);
    ret->len = len;
    if(interface)
    {
        ret->interface = (char *)malloc(sizeof(char)*(strlen(interface) + 1));
        strcpy(ret->interface,interface);
    }
    else
    {
        ret->interface = NULL;
    }

    return ret;
}

void router_free_packet(struct sr_packet * packet)
{
    free(packet->packet);
    if(packet->interface)
    {
        free(packet->interface);
    }
    free(packet);
}

void router_handle_incoming_packet(struct sr_packet * packet)
{
    arp_cache_alert_packet_received(packet);
    switch(ntohs(ETH_HDR(packet)->ether_type))
    {
    case ETHERTYPE_ARP:
        arp_handle_incoming_packet(packet);
        break;
    case ETHERTYPE_IP:
        ip_handle_incoming_packet(packet);
        break;
    }    
}

void router_add_interface(struct sr_instance * sr, struct sr_vns_if * interface)
{
    struct sr_router * router = ROUTER(sr);
    struct ip_address ip;
    interface_list_add_interface(router->iflist,interface);

    ip.subnet = interface->ip & interface->mask;
    ip.mask   = interface->mask;
    forwarding_table_add(sr, &ip, 0, interface->name,1);
    forwarding_table_hw_write(sr);

    if(router->rid == 0)
    {
        router->rid = interface->ip;
    }

    strcpy(router->default_interface,interface->name);
}

void router_create(struct sr_instance * sr, struct sr_options * opt)
{
    NEW_STRUCT(ret,sr_router);
    sr->router = ret;
    ret->ready = 0;

#ifdef _CPUMODE_
    strcpy(ret->device.device_name,DEFAULT_IFACE);
    if(check_iface(&ret->device) == 0)
    {
        if(openDescriptor(&ret->device) < 0)
        {
            fprintf(stderr, "Failed to open device %s", ret->device.device_name);
            exit(1);
        }
    }
    
    writeReg(&ret->device, CPCI_REG_CTRL, 0x00010100);
    usleep(2000);
#endif
    
    interface_list_create(sr);
    forwarding_table_create(sr);
    arp_cache_create(sr);
    arp_reply_waiting_list_create(sr);
    link_state_graph_create(sr);
    nat_create(sr);
    ret->rid = 0;
    ret->ospf_seq = 0;
    ret->opt = *opt;
    ret->ready = 1;

    if(ret->opt.RCPPort != -1)
    {
        ret->rcp_server = RCPSeverCreate(sr, ret->opt.RCPPort);
    }
    else
    {
        ret->rcp_server = 0;
    }
}

void router_destroy(struct sr_router * router)
{
    if(router->rcp_server)
    {
        RCPServerDestroy(router->rcp_server);
    }
    interface_list_destroy(router->iflist);
    forwarding_table_destroy(router->fwd_table);
    arp_cache_destroy(router->a_cache);
    arp_reply_waiting_list_destroy(router->arwl);
    link_state_graph_destroy(router->lsg);
    nat_destroy(router->nat);
    if(router->opt.inbound)
    {
        assoc_array_delete_array(router->opt.inbound, assoc_array_delete_self);
    }
    if(router->opt.outbound)
    {
        assoc_array_delete_array(router->opt.outbound, assoc_array_delete_self);
    }    

#ifdef _CPUMODE_
    closeDescriptor(&router->device);
#endif
    free(router);
}


void router_load_static_routes(struct sr_instance * sr)
{
    struct sr_rt * rt_entry = sr->interface_subsystem->routing_table;
    struct ip_address ip;
        
    while(rt_entry)
    {
        ip.subnet = rt_entry->dest.s_addr;
        ip.mask   = rt_entry->mask.s_addr;
        forwarding_table_add(sr, &ip, rt_entry->gw.s_addr, rt_entry->interface,0);
        rt_entry = rt_entry->next;
    }
}

struct sr_packet * router_copy_packet(struct sr_packet * packet)
{
    return router_construct_packet(packet->sr, packet->packet, packet->len, packet->interface);
}


int router_cmp_MAC(void * k1, void * k2)
{
    uint8_t * i1 = (uint8_t *)k1;
    uint8_t * i2 = (uint8_t *)k2;
    int i = 0;
    for(;i < ETHER_ADDR_LEN; i++)
    {
        if(i1[i] < i2[i])
        {
            return ASSOC_ARRAY_KEY_LT;
        }
        else if(i1[i] > i2[i])
        {
            return ASSOC_ARRAY_KEY_GT;
        }
    }
    return ASSOC_ARRAY_KEY_EQ;
}

void sr_router_default_options(struct sr_options * opt)
{
    opt->arp_proxy = 0;
    opt->aid = 0;

    opt->RCPPort = -1;
    opt->inbound = NULL;
    opt->outbound = NULL;

    opt->verbose = 0;
    opt->show_ip = 0;
    opt->show_arp = 0;
    opt->show_ospf = 0;
    opt->show_ospf_hello = 0;
    opt->show_ospf_lsu = 0;
    opt->show_icmp = 0;
    opt->show_tcp = 0; 
}

int router_nat_enabled(struct sr_instance * sr)
{
    return ((OPTIONS(sr)->inbound != NULL) || (OPTIONS(sr)->outbound != NULL));
}
