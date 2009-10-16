#include <assert.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "grizly.h"
#include <string.h>
#include <stdlib.h>

/*
 * lookup the hardware MAC address corresponding to the IP address ip
 */
int grizly_lookup_arp(struct sr_instance * inst, uint32_t ip, unsigned char * ha)
{
    struct sr_if* if_walker = 0;
    int ret = 0;

    if(inst->if_list != 0)
    {
        if_walker = inst->if_list;
    
        if(if_walker->ip == ip)
        {
            ret = 1;
            memcpy(ha,if_walker->addr,ETHER_ADDR_LEN);
        }
        
        while((ret == 0) && if_walker->next)
        {
            if_walker = if_walker->next;
            
            if(if_walker->ip == ip)
            {
                ret = 1;
                memcpy(ha,if_walker->addr,ETHER_ADDR_LEN);
            }
        }
    }
    
    return ret;
}

void grizly_process_arp_request(struct sr_instance * inst, struct sr_arphdr * request, char * interface)
{
    uint8_t * arp_response = 0;
    struct sr_arphdr * response;
    struct sr_ethernet_hdr * eth_hdr;
    unsigned int len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arphdr);

    arp_response = (uint8_t *) malloc(len);
    response = (struct sr_arphdr *) (arp_response + sizeof(struct sr_ethernet_hdr));
    eth_hdr = (struct sr_ethernet_hdr *)arp_response;
    memcpy(response,request,sizeof(struct sr_arphdr));

    if(grizly_lookup_arp(inst,request->ar_tip,response->ar_sha))
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
