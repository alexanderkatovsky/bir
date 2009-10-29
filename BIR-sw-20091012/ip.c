#include "router.h"
#include "debug.h"
#include "arp_cache.h"
#include <stdlib.h>

void sr_transport_input(uint8_t* packet /* borrowed */);

void ip_forward_packet(struct sr_packet * packet, uint32_t next_hop, const char * thru_interface)
{
    struct ip * ip_hdr = IP_HDR(packet);
    struct sr_ethernet_hdr * eth_hdr = ETH_HDR(packet);
    
    Debug("forwarding packet to "); dump_ip(next_hop);
    Debug(" through interface %s \n", thru_interface);

    if(!arp_cache_get_MAC_from_ip(ROUTER(packet->sr)->a_cache, next_hop, eth_hdr->ether_dhost))
    {
        arp_request_handler_make_request(packet,next_hop,thru_interface);
    }
    else
    {
        ip_hdr->ip_ttl --;
        ip_hdr->ip_sum = checksum_ipheader(ip_hdr);

        if(interface_list_get_MAC_and_IP_from_name(ROUTER(packet->sr)->iflist,thru_interface,eth_hdr->ether_shost,NULL))
        {
            if(sr_integ_low_level_output(packet->sr,packet->packet,packet->len,thru_interface) == -1)
            {
                printf("\nfailed to send packet\n");
            }
        }
    }
}

void ip_forward(struct sr_packet * packet)
{
    struct ip * ip_hdr = IP_HDR(packet);
    uint32_t next_hop;
    char thru_interface[SR_NAMELEN];
    
    if(forwarding_table_lookup_next_hop(ROUTER(packet->sr)->fwd_table,ip_hdr->ip_dst.s_addr, &next_hop, thru_interface))
    {
        ip_forward_packet(packet,next_hop,thru_interface);
    }
    else
    {
        icmp_send_host_unreachable(packet);
    }
}

void ip_handle_incoming_packet(struct sr_packet * packet)
{
    struct ip * ip_hdr = IP_HDR(packet);

    if(ip_hdr->ip_sum != checksum_ipheader(ip_hdr))
    {
        printf("\nchecksums differ %x, %x\n", ip_hdr->ip_sum, checksum_ipheader(ip_hdr));
    }
    else if(ip_hdr->ip_v != 4)
    {
        printf("\nip version is %d; only accepting 4\n",ip_hdr->ip_v);
    }
    else if(ip_hdr->ip_ttl <= 1)
    {
        icmp_send_time_exceeded(packet);
    }
    else
    {
        /* if packet is not for one of our interfaces then forward */
        if(!interface_list_ip_exists(ROUTER(packet->sr)->iflist, ip_hdr->ip_dst.s_addr))
        {
            ip_forward(packet);
        }
        else
        {
            switch(ip_hdr->ip_p)
            {
            case IP_P_ICMP:
                icmp_handle_incoming_packet(packet);
                break;
            case IP_P_TCP:
                sr_transport_input(packet->packet);
                break;
            default:
                icmp_send_port_unreachable(packet);
            }
        }
    }    
}
