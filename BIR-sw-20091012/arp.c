#include "router.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>

void arp_request(struct sr_instance * sr, uint32_t ip, char * interface)
{
    int len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arphdr);
    uint8_t * data = (uint8_t *)malloc(len);
    struct sr_ethernet_hdr * eth_hdr = (struct sr_ethernet_hdr *) data;
    struct sr_arphdr * arp_hdr = (struct sr_arphdr *) (data + sizeof(struct sr_ethernet_hdr));
    int i = 0;

    eth_hdr->ether_type = htons(ETHERTYPE_ARP);

    for(; i < ETHER_ADDR_LEN; i++)
    {
        eth_hdr->ether_dhost[i] = 0xff;
    }

    if(!interface_list_get_MAC_and_IP_from_name(ROUTER(sr)->iflist,
                                                interface,eth_hdr->ether_shost,&(arp_hdr->ar_sip)))
    {
        printf("unable to get MAC from interface %s\n", interface);
    }
    else
    {
        arp_hdr->ar_hrd = htons(1);
        arp_hdr->ar_pro = htons(0x800);
        arp_hdr->ar_hln = ETHER_ADDR_LEN;
        arp_hdr->ar_pln = 4;
        arp_hdr->ar_op  = htons(ARP_REQUEST);
        memcpy(arp_hdr->ar_sha,eth_hdr->ether_shost,ETHER_ADDR_LEN);
        memcpy(arp_hdr->ar_tha,eth_hdr->ether_dhost,ETHER_ADDR_LEN);
        arp_hdr->ar_tip = ip;

/*        printf("ARP request to "); dump_ip(ip);printf(" through interface %s\n", interface);*/

        if(sr_integ_low_level_output(sr,data,len,interface) == -1)
        {
            printf("\nfailed to send packet\n");
        }
    }
}

void arp_reply_to_request(struct sr_packet * packet)
{
    struct sr_arphdr * arp_hdr = ARP_HDR(packet);
    struct sr_ethernet_hdr * eth_hdr = ETH_HDR(packet);
    struct sr_vns_if * vnsif = interface_list_get_interface_by_ip(ROUTER(packet->sr)->iflist, arp_hdr->ar_tip);
    uint32_t temp;
    
    if(vnsif)
    {
        arp_hdr->ar_op = htons(ARP_REPLY);
        temp = arp_hdr->ar_tip;
        arp_hdr->ar_tip = arp_hdr->ar_sip;
        arp_hdr->ar_sip = temp;
        memcpy(arp_hdr->ar_tha,arp_hdr->ar_sha,ETHER_ADDR_LEN);
        memcpy(arp_hdr->ar_sha,vnsif->addr,ETHER_ADDR_LEN);
        memcpy(eth_hdr->ether_dhost,eth_hdr->ether_shost,ETHER_ADDR_LEN);
        memcpy(eth_hdr->ether_shost,vnsif->addr,ETHER_ADDR_LEN);

        if(sr_integ_low_level_output(packet->sr,packet->packet,packet->len,packet->interface) == -1)
        {
            printf("\nfailed to send packet\n");
        }        
    }
}

void arp_handle_incoming_packet(struct sr_packet * packet)
{
    switch(ntohs(ARP_HDR(packet)->ar_op))
    {
    case ARP_REQUEST:
        arp_reply_to_request(packet);
        break;
    case ARP_REPLY:
        arp_request_handler_process_reply(packet);
        break;
    }
}
