#include "router.h"
#include <string.h>
#include <stdlib.h>
#include "sr_ifsys.h"

struct sr_packet * router_construct_packet(struct sr_instance * sr, const uint8_t * packet, unsigned int len, const char* interface)
{
    struct sr_packet * ret = (struct sr_packet *)malloc(sizeof(struct sr_packet));

    ret->sr = sr;
    ret->packet = (uint8_t *) malloc(len*sizeof(uint8_t));
    memcpy(ret->packet,packet,len);
    ret->len = len;
    ret->interface = (char *)malloc(sizeof(char)*(strlen(interface) + 1));
    strcpy(ret->interface,interface);

    return ret;
}

void router_free_packet(struct sr_packet * packet)
{
    free(packet->packet);
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
    interface_list_add_interface(ROUTER(sr)->iflist,interface);
}

struct sr_router * router_create()
{
    NEW_STRUCT(ret,sr_router);
    ret->iflist = interface_list_create();
    ret->fwd_table = forwarding_table_create();
    ret->a_cache = arp_cache_create();
    ret->arwl = arp_reply_waiting_list_create();
    return ret;
}

void router_destroy(struct sr_router * router)
{
    interface_list_destroy(router->iflist);
    forwarding_table_destroy(router->fwd_table);
    arp_cache_destroy(router->a_cache);
    arp_reply_waiting_list_destroy(router->arwl);

    free(router);
}


/*
 * Swaps destination and source MAC addresses and sends
 * */
void router_swap_eth_header_and_send(struct sr_packet * packet)
{
    uint8_t buf[ETHER_ADDR_LEN];
    struct sr_ethernet_hdr * eth_hdr = ETH_HDR(packet);

    memcpy(buf,eth_hdr->ether_dhost,ETHER_ADDR_LEN);
    memcpy(eth_hdr->ether_dhost,eth_hdr->ether_shost,ETHER_ADDR_LEN);
    memcpy(eth_hdr->ether_shost,buf,ETHER_ADDR_LEN);

    if(sr_integ_low_level_output(packet->sr,packet->packet,packet->len,packet->interface) == -1)
    {
        printf("\nfailed to send packet\n");
    }
}

void router_load_static_routes(struct sr_instance * sr)
{
    struct sr_rt * rt_entry = sr->interface_subsystem->routing_table;
    struct forwarding_table * fwd_table = ROUTER(sr)->fwd_table;
        
    while(rt_entry)
    {
        forwarding_table_add_static_route(fwd_table,rt_entry);
        rt_entry = rt_entry->next;
    }
}

struct sr_packet * router_copy_packet(struct sr_packet * packet)
{
    struct sr_packet * ret = (struct sr_packet *)malloc(sizeof(struct sr_packet));
    *ret = *packet;
    ret->packet = (uint8_t *)malloc(packet->len);
    memcpy(ret->packet,packet->packet,packet->len);
    return ret;
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
