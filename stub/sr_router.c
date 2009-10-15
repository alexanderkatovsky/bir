/**********************************************************************
 * file:  sr_router.c 
 * date:  Mon Feb 18 12:50:42 PST 2002  
 * Contact: casado@stanford.edu 
 *
 * Description:
 * 
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/
#include <assert.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "grizly.h"
#include <string.h>
#include <stdlib.h>
/*--------------------------------------------------------------------- 
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 * 
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr) 
{
    /* REQUIRES */
    assert(sr);

    /* Add initialization code here! */

} /* -- sr_init -- */

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


/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr, 
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
    struct sr_ethernet_hdr * eth_hdr = 0;
    uint16_t frame_type;
    
    /* REQUIRES */
    assert(sr);
    assert(packet);
    assert(interface);

    grizly_dump_packet(packet,len);

    eth_hdr = (struct sr_ethernet_hdr *) packet;
    frame_type = ntohs(eth_hdr->ether_type);

    switch(frame_type)
    {
    case ETHERTYPE_ARP:
        grizly_process_arp_request(sr, (struct sr_arphdr*) (packet + sizeof(struct sr_ethernet_hdr)), interface);
        break;
    }

    printf("*** -> Received packet of length %d \n",len);

    
}/* end sr_ForwardPacket */


/*--------------------------------------------------------------------- 
 * Method:
 *
 *---------------------------------------------------------------------*/
