#include "router.h"
#include <string.h>
#include <stdlib.h>

void icmp_construct_headers(uint8_t * data, struct sr_packet * packet, unsigned int ip_len)
{
    int start_ip = sizeof(struct sr_ethernet_hdr);
    struct sr_ethernet_hdr * eth_from = (struct sr_ethernet_hdr *) (packet->packet);
    struct sr_ethernet_hdr * eth_to = (struct sr_ethernet_hdr *) (data);
    struct ip * ip_from = (struct ip *) (packet->packet + start_ip);
    struct ip * ip_to = (struct ip *) (data + start_ip);

    interface_list_get_MAC_and_IP_from_name(ROUTER(packet->sr)->iflist,
                                            packet->interface,eth_to->ether_shost,&ip_to->ip_src.s_addr);

    memcpy(eth_to->ether_dhost,eth_from->ether_shost,ETHER_ADDR_LEN);
    eth_to->ether_type = htons(ETHERTYPE_IP);

    ip_to->ip_hl = 5;
    ip_to->ip_v = 4;
    ip_to->ip_tos = 0;
    ip_to->ip_len = htons(ip_len);
    ip_to->ip_id = ip_from->ip_id;
    ip_to->ip_off = 0;
    ip_to->ip_dst = ip_from->ip_src;
    ip_to->ip_p = IP_P_ICMP;
    ip_to->ip_ttl = 63;
    ip_to->ip_sum = checksum_ipheader(ip_to);
}

void icmp_basic(struct sr_packet * packet, int prot, int code)
{
    int len = sizeof(struct sr_ethernet_hdr) + 2*sizeof(struct ip) + sizeof(struct icmphdr) + 8;
    uint8_t * data = (uint8_t*)malloc(len);
    int start_icmp = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
    struct icmphdr * icmp_to = (struct icmphdr *) (data + start_icmp);
    int start_data = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct icmphdr);

    icmp_construct_headers(data,packet,len - sizeof(struct sr_ethernet_hdr));
    icmp_to->type = prot;
    icmp_to->code = code;
    icmp_to->checksum = checksum_icmpheader(data + start_icmp, len - start_icmp);
    icmp_to->id = 0;
    icmp_to->sequence = 0;
    memcpy(data + start_data,packet->packet + sizeof(struct sr_ethernet_hdr),len - start_data);

    if(sr_integ_low_level_output(packet->sr,data,len,packet->interface) == -1)
    {
        printf("\nfailed to send packet\n");
    }
    
    free(data);
}

void icmp_send_port_unreachable(struct sr_packet * packet)
{
    icmp_basic(packet,3,3);
}

void icmp_send_time_exceeded(struct sr_packet * packet)
{
    icmp_basic(packet,11,0);
}

void icmp_send_no_route(struct sr_packet * packet)
{
    icmp_basic(packet,3,1);
}


void icmp_reply(struct sr_packet * packet)
{
    uint8_t * data = (uint8_t*)malloc(packet->len);
    int start_icmp = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
    struct icmphdr * icmp_to = (struct icmphdr *) (data + start_icmp);
    memcpy(data,packet->packet,packet->len);
    icmp_construct_headers(data,packet,packet->len - sizeof(struct sr_ethernet_hdr));
    icmp_to->type = ICMP_REPLY;
    icmp_to->code = 0;
    icmp_to->checksum = checksum_icmpheader(data + start_icmp, packet->len - start_icmp);

    if(sr_integ_low_level_output(packet->sr,data,packet->len,packet->interface) == -1)
    {
        printf("\nfailed to send packet\n");
    }

    free(data);
}

void icmp_handle_incoming_packet(struct sr_packet * packet)
{
    int start_icmp = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
    struct icmphdr * icmp_hdr = (struct icmphdr *) (packet->packet + start_icmp);
    
    if(icmp_hdr->checksum != checksum_icmpheader(packet->packet + start_icmp, packet->len - start_icmp))
    {
        printf("\nchecksums differ %x, %x\n", icmp_hdr->checksum,
               checksum_icmpheader(packet->packet + start_icmp, packet->len - start_icmp));
    }
    else
    {
        switch(icmp_hdr->type)
        {
        case ICMP_REQUEST:
            icmp_reply(packet);
            break;
        }
    }
}
