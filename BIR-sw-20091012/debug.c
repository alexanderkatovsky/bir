#include "eth_headers.h"
#include "debug.h"
#include "sr_base_internal.h"
#include <stdio.h>


void dump_packet(const uint8_t * packet, unsigned int len)
{
    int i=0;

    while(i < len)
    {
        if(i % 16 == 0)
        {
            Debug("\n");
        }

        if(i < len - 1)
        {
            Debug("%02x%02x ",packet[i],packet[i+1]);
            i+=2;
        }
        else
        {
            Debug("%02x ",packet[i]);
            i++;
        }
    }

    Debug("\n");
}

void dump_mac(const uint8_t * mac)
{
    int i = 0;
    for(; i < ETHER_ADDR_LEN - 1; i++)
    {
        Debug("%02x:", mac[i]);
    }

    Debug("%02x", mac[ETHER_ADDR_LEN - 1]);
}

void dump_ethernet_hdr(const struct sr_ethernet_hdr * eth_hdr)
{
    Debug("ethernet header:\n");
    Debug("    dest MAC   :   ");dump_mac(eth_hdr->ether_dhost);Debug("\n");
    Debug("    source MAC :   ");dump_mac(eth_hdr->ether_shost);Debug("\n");
    Debug("    type       :   %04x\n", ntohs(eth_hdr->ether_type));
}

void dump_ip(uint32_t ip)
{
    uint8_t * i = (uint8_t *)&ip;
    Debug("%d.%d.%d.%d",i[0],i[1],i[2],i[3]);
}

void dump_arp_hdr(const uint8_t * packet, unsigned int len)
{
    struct sr_arphdr * arp_hdr = (struct sr_arphdr *) (packet + sizeof(struct sr_ethernet_hdr));
    Debug("ARP header     :\n");
    Debug("    ARP op code:   %04x\n",ntohs(arp_hdr->ar_op));
    Debug("    source MAC :   ");dump_mac(arp_hdr->ar_sha);Debug("\n");
    Debug("    source IP  :   ");dump_ip(arp_hdr->ar_sip);Debug("\n");
    Debug("    dest   MAC :   ");dump_mac(arp_hdr->ar_tha);Debug("\n");
    Debug("    dest   IP  :   ");dump_ip(arp_hdr->ar_tip);Debug("\n");
}

void dump_icmp_hdr(const uint8_t * packet, unsigned int len)
{
    struct icmphdr * icmp_hdr = (struct icmphdr *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
    Debug("ICMP header    :\n");
    Debug("    type       :   %02x\n",icmp_hdr->type);
    Debug("    code       :   %02x\n",icmp_hdr->code);
}

void dump_tcp_hdr(const uint8_t * packet, unsigned int len)
{
    struct tcpheader * tcp_hdr = (struct tcpheader *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
    Debug("TCP header     :\n");
    Debug("    source port:   %04x\n",ntohs(tcp_hdr->tcph_srcport));
    Debug("    dest port  :   %04x\n",ntohs(tcp_hdr->tcph_destport));
}

void dump_ip_hdr(const uint8_t * packet, unsigned int len)
{
    struct ip * ip_hdr = (struct ip *) (packet + sizeof(struct sr_ethernet_hdr));
    Debug("IP header      :\n");
    Debug("    version    :   %01x\n",ip_hdr->ip_v);
    Debug("    ttl        :   %d\n",ip_hdr->ip_ttl);
    Debug("    protocol   :   %02x\n",ip_hdr->ip_p);
    Debug("    source IP  :   ");dump_ip(ip_hdr->ip_src.s_addr);Debug("\n");
    Debug("    dest   IP  :   ");dump_ip(ip_hdr->ip_dst.s_addr);Debug("\n");

    switch(ip_hdr->ip_p)
    {
    case IP_P_ICMP:
        dump_icmp_hdr(packet,len);
        break;
    case IP_P_TCP:
        dump_tcp_hdr(packet,len);
        break;
    }
}

void dump_raw(const uint8_t * packet, unsigned int len)
{
    struct sr_ethernet_hdr * eth_hdr = (struct sr_ethernet_hdr *)packet;
    dump_packet(packet,len);
    dump_ethernet_hdr(eth_hdr);

    switch(ntohs(eth_hdr->ether_type))
    {
    case ETHERTYPE_ARP:
        dump_arp_hdr(packet,len);
        break;
    case ETHERTYPE_IP:
        dump_ip_hdr(packet,len);
        break;
    }
}
