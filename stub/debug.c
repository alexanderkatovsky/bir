#include "grizly.h"

void dump_mac(uint8_t * mac)
{
    int i = 0;
    for(; i < ETHER_ADDR_LEN - 1; i++)
    {
        printf("%02x:", mac[i]);
    }

    printf("%02x", mac[ETHER_ADDR_LEN - 1]);
}

void dump_ethernet_hdr(struct sr_ethernet_hdr * eth_hdr)
{
    printf("ethernet header:\n");
    printf("    dest MAC   :   ");dump_mac(eth_hdr->ether_dhost);printf("\n");
    printf("    source MAC :   ");dump_mac(eth_hdr->ether_shost);printf("\n");
    printf("    type       :   %04x\n", ntohs(eth_hdr->ether_type));
}

void dump_ip(uint32_t ip)
{
    uint8_t * i = (uint8_t *)&ip;
    printf("%d.%d.%d.%d",i[0],i[1],i[2],i[3]);
}

void dump_arp_hdr(uint8_t * packet, unsigned int len)
{
    struct sr_arphdr * arp_hdr = (struct sr_arphdr *) (packet + sizeof(struct sr_ethernet_hdr));
    printf("ARP header     :\n");
    printf("    ARP op code:   %04x\n",ntohs(arp_hdr->ar_op));
    printf("    source MAC :   ");dump_mac(arp_hdr->ar_sha);printf("\n");
    printf("    source IP  :   ");dump_ip(arp_hdr->ar_sip);printf("\n");
    printf("    dest   MAC :   ");dump_mac(arp_hdr->ar_tha);printf("\n");
    printf("    dest   IP  :   ");dump_ip(arp_hdr->ar_tip);printf("\n");
}

void dump_icmp_hdr(uint8_t * packet, unsigned int len)
{
    struct icmphdr * icmp_hdr = (struct icmphdr *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
    printf("ICMP header    :\n");
    printf("    type       :   %02x\n",icmp_hdr->type);
}

void dump_tcp_hdr(uint8_t * packet, unsigned int len)
{
    struct tcpheader * tcp_hdr = (struct tcpheader *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
    printf("TCP header     :\n");
    printf("    source port:   %04x\n",ntohs(tcp_hdr->tcph_srcport));
    printf("    dest port  :   %04x\n",ntohs(tcp_hdr->tcph_destport));
}

void dump_ip_hdr(uint8_t * packet, unsigned int len)
{
    struct ip * ip_hdr = (struct ip *) (packet + sizeof(struct sr_ethernet_hdr));
    printf("IP header      :\n");
    printf("    version    :   %01x\n",ip_hdr->ip_v);
    printf("    ttl        :   %d\n",ip_hdr->ip_ttl);
    printf("    protocol   :   %02x\n",ip_hdr->ip_p);
    printf("    source IP  :   ");dump_ip(ip_hdr->ip_src.s_addr);printf("\n");
    printf("    dest   IP  :   ");dump_ip(ip_hdr->ip_dst.s_addr);printf("\n");

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

void dump_raw(uint8_t * packet, unsigned int len)
{
    struct sr_ethernet_hdr * eth_hdr = (struct sr_ethernet_hdr *)packet;
    grizly_dump_packet(packet,len);
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
