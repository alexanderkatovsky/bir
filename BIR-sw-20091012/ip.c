#include "router.h"
#include "debug.h"
#include "arp_cache.h"
#include "common.h"

void sr_transport_input(uint8_t* packet /* borrowed */);

void ip_forward_packet(struct sr_packet * packet, uint32_t next_hop, char * thru_interface)
{
    struct ip * ip_hdr = IP_HDR(packet);
    struct sr_ethernet_hdr * eth_hdr = ETH_HDR(packet);

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

    /* if the destination is to one of our ports, then just forward it there  */
    if(interface_list_get_interface_by_ip(INTERFACE_LIST(packet->sr), ip_hdr->ip_dst.s_addr))
    {
        ip_handle_incoming_packet(packet);
    }
    else
    {
        if(forwarding_table_lookup_next_hop(ROUTER(packet->sr)->fwd_table,
                                            ip_hdr->ip_dst.s_addr, &next_hop, thru_interface))
        {
            if(next_hop == 0)
            {
                next_hop = ip_hdr->ip_dst.s_addr;
            }
            ip_forward_packet(packet,next_hop,thru_interface);
        }
        else
        {
            dump_ip(ip_hdr->ip_dst.s_addr);Debug(" not in forwarding table\n");
            if(forwarding_table_lookup_next_hop(ROUTER(packet->sr)->fwd_table,
                                                ip_hdr->ip_src.s_addr, &next_hop, thru_interface))
            {
                icmp_send_host_unreachable(packet);
            }
        }
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
    else
    {
        /* if packet is not for (or from) one of our interfaces then forward */
        if(!interface_list_ip_exists(ROUTER(packet->sr)->iflist, ip_hdr->ip_dst.s_addr) &&
           !interface_list_ip_exists(ROUTER(packet->sr)->iflist, ip_hdr->ip_src.s_addr) &&
           ntohl(ip_hdr->ip_dst.s_addr) != OSPF_AllSPFRouters)
        {
            if(ip_hdr->ip_ttl <= 1)
            {
                icmp_send_time_exceeded(packet);
            }
            else
            {
                ip_forward(packet);
            }
        }
        else
        {
            switch(ip_hdr->ip_p)
            {
            case IP_P_ICMP:
                icmp_handle_incoming_packet(packet);
                break;
            case IP_P_TCP:
                /*sr_transport_input(packet->packet);*/
                break;
            case IP_P_OSPF:
                ospf_handle_incoming_packet(packet);
                break;
            default:
                icmp_send_port_unreachable(packet);
            }
        }
    }    
}

static const uint8_t ZERO_MAC[ETHER_ADDR_LEN] = {0xff,0xff,0xff,0xff,0xff,0xff};

void ip_construct_eth_header(uint8_t * packet, const uint8_t * dest_MAC, const uint8_t * src_MAC, uint16_t type)
{
    struct sr_ethernet_hdr * eth_hdr = B_ETH_HDR(packet);
    dest_MAC = (dest_MAC == NULL) ? ZERO_MAC : dest_MAC;
    src_MAC = (src_MAC == NULL) ? ZERO_MAC : src_MAC;

    memcpy(eth_hdr->ether_dhost, dest_MAC, ETHER_ADDR_LEN);
    memcpy(eth_hdr->ether_shost, src_MAC, ETHER_ADDR_LEN);
    eth_hdr->ether_type = htons(type);
}

void ip_construct_ip_header(uint8_t * packet, uint16_t len,
                            uint16_t id, uint8_t ttl,
                            uint8_t p, uint32_t ip_src, uint32_t ip_dest)
{
    struct ip * ip_to = B_IP_HDR(packet);

    struct in_addr src = {ip_src};
    struct in_addr dst = {ip_dest};

    uint16_t ip_len = len - sizeof(struct sr_ethernet_hdr);

    ip_to->ip_hl = 5;
    ip_to->ip_v = 4;
    ip_to->ip_tos = 0;
    ip_to->ip_len = htons(ip_len);
    ip_to->ip_id = htons(id);
    ip_to->ip_off = 0;
    ip_to->ip_src = src;
    ip_to->ip_dst = dst;
    ip_to->ip_p = p;
    ip_to->ip_ttl = ttl;
    ip_to->ip_sum = checksum_ipheader(ip_to);
}
