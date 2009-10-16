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

void grizly_process_ip_icmp_request_data(struct sr_instance * sr,uint8_t * packet, unsigned int len,char* interface)
{
    uint8_t * data = (uint8_t*)malloc(len);
    int start_ip = sizeof(struct sr_ethernet_hdr);
    int start_icmp = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
    struct sr_ethernet_hdr * eth_from = (struct sr_ethernet_hdr *) (packet);
    struct sr_ethernet_hdr * eth_to = (struct sr_ethernet_hdr *) (data);
    struct ip * ip_from = (struct ip *) (packet + start_ip);
    struct ip * ip_to = (struct ip *) (data + start_ip);
    struct icmphdr * icmp_from = (struct icmphdr *) (packet + start_icmp);
    struct icmphdr * icmp_to = (struct icmphdr *) (data + start_icmp);
    memcpy(data,packet,len);
    memcpy(eth_to->ether_dhost,eth_from->ether_shost,ETHER_ADDR_LEN);
    memcpy(eth_to->ether_shost,eth_from->ether_dhost,ETHER_ADDR_LEN);

    ip_to->ip_src = ip_from->ip_dst;
    ip_to->ip_dst = ip_from->ip_src;

    icmp_to->type = ICMP_REPLY;
    icmp_to->checksum = icmpheader_checksum(packet + start_icmp, len - start_icmp);

    if(sr_send_packet(sr,data,len,interface) == -1)
    {
        printf("\nfailed to send ARP response\n");
    }

    free(data);
}

void grizly_process_ip_icmp_data(struct sr_instance * sr,uint8_t * packet, unsigned int len,char* interface)
{
    int start_icmp = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
    struct icmphdr * icmp_hdr = (struct icmphdr *) (packet + start_icmp);

    if(icmp_hdr->checksum != icmpheader_checksum(packet + start_icmp, len - start_icmp))
    {
        printf("\nchecksums differ %x, %x\n", icmp_hdr->checksum,
               icmpheader_checksum(packet + start_icmp, len - start_icmp));
    }
    else
    {
        switch(icmp_hdr->type)
        {
        case ICMP_REQUEST:
            grizly_process_ip_icmp_request_data(sr,packet,len,interface);
            break;
        }
    }
}

void grizly_process_ip_data(struct sr_instance * sr,uint8_t * packet, unsigned int len,char* interface)
{
    struct ip * ip_hdr = (struct ip * ) (packet + sizeof(struct sr_ethernet_hdr));

    if(ip_hdr->ip_sum != ipheader_checksum(ip_hdr))
    {
        printf("\nchecksums differ %x, %x\n", ip_hdr->ip_sum, ipheader_checksum(ip_hdr));
    }
    else
    {
        switch(ip_hdr->ip_p)
        {
        case IP_P_ICMP:
            grizly_process_ip_icmp_data(sr,packet,len,interface);
            break;
        }
    }
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
    case ETHERTYPE_IP:
        grizly_process_ip_data(sr,packet,len,interface);
        break;
    }

    printf("*** -> Received packet of length %d \n",len);    
}/* end sr_ForwardPacket */


/*--------------------------------------------------------------------- 
 * Method:
 *
 *---------------------------------------------------------------------*/
