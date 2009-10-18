#include <assert.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "grizly.h"
#include <string.h>
#include <stdlib.h>

int get_MAC_from_interface_name(struct sr_instance * inst, char * interface, unsigned char * ha)
{
    uint32_t temp;

    return get_MAC_and_IP_from_interface_name(inst,interface,ha,&temp);
}

int get_MAC_and_IP_from_interface_name(struct sr_instance * inst, char * interface, unsigned char * ha, uint32_t * ip)
{
    struct sr_if * node = inst->if_list;
    int ret = 0;

    while(!ret && node)
    {
        if(strcmp(interface, node->name) == 0)
        {
            ret = 1;
            memcpy(ha,node->addr,ETHER_ADDR_LEN);
            *ip = node->ip;
        }

        node = node->next;
    }

    return ret;
}

/*
 * lookup the hardware MAC address corresponding to the IP address ip
 */
int get_MAC_from_interface_ip(struct sr_instance * inst, uint32_t ip, unsigned char * ha)
{

    struct sr_if * node = inst->if_list;
    int ret = 0;

    while(!ret && node)
    {
        if(node->ip == ip)
        {
            ret = 1;
            memcpy(ha,node->addr,ETHER_ADDR_LEN);
        }

        node = node->next;
    }

    return ret;
}

void arp_reply_to_request(struct sr_instance * inst, struct sr_arphdr * request, char * interface)
{
    uint8_t * arp_response = 0;
    struct sr_arphdr * response;
    struct sr_ethernet_hdr * eth_hdr;
    unsigned int len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arphdr);

    arp_response = (uint8_t *) malloc(len);
    response = (struct sr_arphdr *) (arp_response + sizeof(struct sr_ethernet_hdr));
    eth_hdr = (struct sr_ethernet_hdr *)arp_response;
    memcpy(response,request,sizeof(struct sr_arphdr));

    if(get_MAC_from_interface_ip(inst,request->ar_tip,response->ar_sha))
    {
        response->ar_op = htons(ARP_REPLY);
        memcpy(response->ar_tha,request->ar_sha,ETHER_ADDR_LEN);
        response->ar_tip = request->ar_sip;
        response->ar_sip = request->ar_tip;
        
        memcpy(eth_hdr->ether_dhost,request->ar_sha,ETHER_ADDR_LEN);
        memcpy(eth_hdr->ether_shost,response->ar_sha,ETHER_ADDR_LEN);
        eth_hdr->ether_type = htons(ETHERTYPE_ARP);

        if(sr_send_packet(inst,arp_response,len,interface) == -1)
        {
            printf("\nfailed to send ARP response\n");
        }
    }

    free(arp_response);
}

void process_arp_reply(struct sr_instance * inst, struct sr_arphdr * reply)
{
    add_to_arp_cache(reply->ar_sip,reply->ar_sha);

    /* forward everything in the wfar list */
    process_wfar_list_on_arp_reply(reply->ar_sip);
}

void process_arp_data(struct sr_instance * inst, struct sr_arphdr * request, char * interface)
{
    switch(ntohs(request->ar_op))
    {
    case ARP_REQUEST:
        arp_reply_to_request(inst,request,interface);
        break;
    case ARP_REPLY:
        process_arp_reply(inst,request);
        break;
    }
}
