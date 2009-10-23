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

    printf("address to server: ");dump_ip(sr->sr_addr.sin_addr.s_addr);printf("\n");
    printf("user name        : %s\n",sr->user);
    printf("host name        : %s\n",sr->host);

    init_arp_cache();
    init_wfar_list();

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

void process_tcp_data_for_us(struct sr_instance * sr,uint8_t * packet, unsigned int len,char* interface)
{
    struct ip * ip_hdr = (struct ip * ) (packet + sizeof(struct sr_ethernet_hdr));

    if(ip_hdr->ip_ttl <= 1)
    {
        send_icmp_time_exceeded(sr,packet,len,interface);
    }
}

/*is the ip address one of the router's interfaces?*/
int my_ip(struct sr_instance * sr, uint32_t ip)
{
    int ret = 0;
    struct sr_if * if_list = sr->if_list;

    if(if_list->ip == ip)
    {
        ret = 1;
    }

    while(!ret && (if_list = if_list->next))
    {
        if(if_list->ip == ip)
        {
            ret = 1;
        }
    }

    return ret;
}


int LPM(struct sr_instance * sr, uint32_t dest_ip, uint32_t * next_hop, char ** thru_interface)
{
    int ret = 0;
    int max_mask = 0;
    int mask;

    struct sr_rt * rt = sr->routing_table;

    do
    {
        mask = rt->mask.s_addr;
        if((dest_ip & mask) == (rt->dest.s_addr & mask))
        {
            if((mask & max_mask) == max_mask)
            {
                ret = 1;
                max_mask = mask;
                *next_hop = rt->gw.s_addr;
                *thru_interface = rt->interface;
            }
        }
    } while((rt = rt->next));

    return ret;
}

void send_icmp_host_unreachable(struct sr_instance * sr,uint8_t * packet, unsigned int len,char* interface)
{
    
}

void send_icmp_time_exceeded(struct sr_instance * sr,uint8_t * packet, unsigned int len,char* interface)
{
    int len = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof();
    printf("sending ICMP time exceeded\n");

}

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

    if(!get_MAC_and_IP_from_interface_name(sr,interface,eth_hdr->ether_shost,&(arp_hdr->ar_sip)))
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

        printf("ARP request to "); dump_ip(ip);printf(" through interface %s\n", interface);

        if(sr_send_packet(sr,data,len,interface) == -1)
        {
            printf("\nfailed to send arp request\n");
        }
    }
}

void forward_packet(struct sr_instance * sr,uint8_t * packet, unsigned int len, uint32_t next_hop, char* interface)
{
    struct ip * ip_hdr = (struct ip * ) (packet + sizeof(struct sr_ethernet_hdr));
    struct sr_ethernet_hdr * eth_hdr = (struct sr_ethernet_hdr *) packet;
    
    printf("\nforwarding packet to "); dump_ip(next_hop);
    printf(" through interface %s\n",interface);
    
    if(!get_MAC_from_arp_cache(next_hop, eth_hdr->ether_dhost))
    {
        arp_request(sr,next_hop,interface);
        add_to_wfar_list(sr,packet,len,next_hop,interface);
    }
    else
    {
        ip_hdr->ip_ttl --;
        ip_hdr->ip_sum = ipheader_checksum(ip_hdr);

        if(get_MAC_from_interface_name(sr,interface,eth_hdr->ether_shost))
        {
            if(sr_send_packet(sr,packet,len,interface) == -1)
            {
                printf("\nfailed to forward packet\n");
            }
        }
    }   
}

void forward(struct sr_instance * sr,uint8_t * packet, unsigned int len,char* interface)
{
    struct ip * ip_hdr = (struct ip * ) (packet + sizeof(struct sr_ethernet_hdr));
    uint32_t next_hop;
    char * thru_interface;
    if(ip_hdr->ip_ttl <= 1)
    {
        send_icmp_time_exceeded(sr,packet,len,interface);
    }
    else if(LPM(sr,ip_hdr->ip_dst.s_addr, &next_hop, &thru_interface))
    {
        forward_packet(sr,packet,len,next_hop,thru_interface);
    }
    else
    {
        send_icmp_host_unreachable(sr,packet,len,interface);
    }
}

void grizly_process_ip_data(struct sr_instance * sr,uint8_t * packet, unsigned int len,char* interface)
{
    struct ip * ip_hdr = (struct ip * ) (packet + sizeof(struct sr_ethernet_hdr));

    if(ip_hdr->ip_sum != ipheader_checksum(ip_hdr))
    {
        printf("\nchecksums differ %x, %x\n", ip_hdr->ip_sum, ipheader_checksum(ip_hdr));
    }
    else if(ip_hdr->ip_v != 4)
    {
        printf("\nip version is %d; only accepting 4\n",ip_hdr->ip_v);
    }
    else
    {
        if(!my_ip(sr,ip_hdr->ip_dst.s_addr))
        {
            forward(sr,packet,len,interface);
        }
        else
        {
            switch(ip_hdr->ip_p)
            {
            case IP_P_ICMP:
                grizly_process_ip_icmp_data(sr,packet,len,interface);
                break;
            case IP_P_TCP:
                process_tcp_data_for_us(sr,packet,len,interface);
                break;
            }
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

    printf("\n>>>> Received packet of length %d \n",len);
    dump_raw(packet,len);

    eth_hdr = (struct sr_ethernet_hdr *) packet;
    frame_type = ntohs(eth_hdr->ether_type);

    switch(frame_type)
    {
    case ETHERTYPE_ARP:
        process_arp_data(sr, (struct sr_arphdr*) (packet + sizeof(struct sr_ethernet_hdr)), interface);
        break;
    case ETHERTYPE_IP:
        grizly_process_ip_data(sr,packet,len,interface);
        break;
    }
}/* end sr_ForwardPacket */


/*--------------------------------------------------------------------- 
 * Method:
 *
 *---------------------------------------------------------------------*/
