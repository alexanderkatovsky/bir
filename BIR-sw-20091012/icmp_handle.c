#include "router.h"
#include <string.h>
#include <stdlib.h>
#include "eth_headers.h"
#include "cli/cli.h"

void icmp_construct_header(uint8_t * data, uint8_t type, uint8_t code, uint16_t id, uint16_t sequence)
{
    int start_icmp = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
    struct icmphdr * icmp_h = (struct icmphdr *) (data + start_icmp);

    icmp_h->type = type;
    icmp_h->code = code;
    icmp_h->id = id;
    icmp_h->sequence = sequence;
    icmp_h->checksum = checksum_icmpheader((uint8_t*)icmp_h,sizeof(struct icmphdr));
}

/* for icmp messages that require the first 64 bits of the original message */
void icmp_basic_reply(struct sr_packet * packet, int prot, int code)
{
    int len = sizeof(struct sr_ethernet_hdr) + 2*sizeof(struct ip) + sizeof(struct icmphdr) + 8;
    uint8_t * data = (uint8_t*)malloc(len);
    struct ip * iph = IP_HDR(packet);
    struct sr_packet * packet2;
    int start_data = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct icmphdr);
    memcpy(data + start_data,packet->packet + sizeof(struct sr_ethernet_hdr),len - start_data);
    icmp_construct_header(data, prot, code, 0, 0);
    ip_construct_ip_header(data,len,0,63,IP_P_ICMP,iph->ip_dst.s_addr,iph->ip_src.s_addr);
    ip_construct_eth_header(data,0,0,ETHERTYPE_IP);
    packet2 = router_construct_packet(packet->sr,data,len,"");
    ip_forward(packet2);
    router_free_packet(packet2);        
    
    free(data);
}

void icmp_send_port_unreachable(struct sr_packet * packet)
{
    icmp_basic_reply(packet,ICMP_DEST_UNREACH,3);
}

void icmp_send_time_exceeded(struct sr_packet * packet)
{
    icmp_basic_reply(packet,ICMP_TIME_EXCEEDED,0);
}

void icmp_send_host_unreachable(struct sr_packet * packet)
{
    icmp_basic_reply(packet,ICMP_DEST_UNREACH,1);
}


void icmp_send_ping(struct sr_instance * sr, uint32_t ip, uint32_t seq_num)
{
    int len = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) +
        sizeof(struct icmphdr);
    struct sr_packet * packet;

    uint8_t * data = (uint8_t *)malloc(len);
    icmp_construct_header(data,8,0,0,seq_num);
    ip_construct_ip_header(data,len,0,63,IP_P_ICMP,ROUTER(sr)->rid,ip);
    ip_construct_eth_header(data,0,0,ETHERTYPE_IP);
    packet = router_construct_packet(sr,data,len,"");
    ip_forward(packet);
    router_free_packet(packet);
    free(data);
}

void icmp_reply(struct sr_packet * packet)
{
    int len = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) +
        sizeof(struct icmphdr);
    uint8_t * data = (uint8_t*)malloc(len);
    struct ip * iph = IP_HDR(packet);
    struct icmphdr * icmp_h = ICMP_HDR(packet);
    struct sr_packet * packet2;
    
    icmp_construct_header(data,ICMP_REPLY,0,icmp_h->id,icmp_h->sequence);
    ip_construct_ip_header(data,packet->len,0,63,
                           IP_P_ICMP,iph->ip_dst.s_addr,
                           iph->ip_src.s_addr);
    ip_construct_eth_header(data,0,0,ETHERTYPE_IP);
    packet2 = router_construct_packet(packet->sr,data,len,"");
    ip_forward(packet2);
    router_free_packet(packet2);
    free(data);
}

void icmp_handle_reply(struct sr_packet * packet)
{
    cli_ping_reply(IP_HDR(packet)->ip_src.s_addr);
}

void icmp_dest_unreach(struct sr_packet * packet)
{
    struct ip * iph = (struct ip *)(packet->packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) +
                       sizeof(struct icmphdr));
    cli_dest_unreach(iph);
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
        case ICMP_REPLY:
            icmp_handle_reply(packet);
            break;
        case ICMP_DEST_UNREACH:
            icmp_dest_unreach(packet);
            break;
        }
    }
}
