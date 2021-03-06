#include "eth_headers.h"
#include "pwospf_protocol.h"
#include "debug.h"
#include "sr_base_internal.h"
#include <stdio.h>
#include "router.h"

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

void print_mac(const uint8_t * mac,print_t print)
{
    int i = 0;
    for(; i < ETHER_ADDR_LEN - 1; i++)
    {
        print("%02x:", mac[i]);
    }

    print("%02x", mac[ETHER_ADDR_LEN - 1]);
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

void print_ip(uint32_t ip,print_t print)
{
    uint8_t * i = (uint8_t *)&ip;
    char buf[20];
    sprintf(buf,"%d.%d.%d.%d",i[0],i[1],i[2],i[3]);
    print("%--15s",buf);
}

void print_ip_port(uint32_t ip,int port,print_t print)
{
    uint8_t * i = (uint8_t *)&ip;
    char buf[20];
    sprintf(buf,"%d.%d.%d.%d:%d",i[0],i[1],i[2],i[3],port);
    print("%--20s",buf);
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

void dump_ospf_hello(const uint8_t * packet, unsigned int len)
{
    struct ospfv2_hello_hdr * h_h = (struct ospfv2_hello_hdr *)(packet +
                                                                sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)
                                                                + sizeof(struct ospfv2_hdr));
    Debug("OSPF Hello header :\n");
    Debug("    netmask    :   ");dump_ip(h_h->nmask);Debug("\n");
    Debug("    helloint   :   %d\n",ntohs(h_h->helloint));
}

void dump_ospf_lsu(const uint8_t * packet, unsigned int len)
{
    struct ospfv2_lsu_hdr * hdr = (struct ospfv2_lsu_hdr *)(packet +
                                                            sizeof(struct sr_ethernet_hdr) +
                                                            sizeof(struct ip) +
                                                            sizeof(struct ospfv2_hdr));

    int i;
    struct ospfv2_lsu * adv;

    Debug("OSPF LSU header :\n");
    Debug("    seq        :   %d\n",ntohs(hdr->seq));
    Debug("    ttl        :   %d\n",hdr->ttl);
    Debug("    #adv       :   %d\n",ntohl(hdr->num_adv));
    adv = (struct ospfv2_lsu *)(((uint8_t *)hdr) + sizeof(struct ospfv2_lsu_hdr));

    for(i = 0; i < ntohl(hdr->num_adv); i++)
    {
        Debug("OSPF LSU Adv   :\n");
        Debug("    subnet     :  ");dump_ip(adv->subnet);Debug("\n");
        Debug("    mask       :  ");dump_ip(adv->mask);Debug("\n");
        Debug("    rid        :  ");dump_ip(adv->rid);Debug("\n");
        adv += 1;
    }
}

void dump_ospf_hdr(const uint8_t * packet, unsigned int len)
{
    struct ospfv2_hdr * ospf_hdr = (struct ospfv2_hdr *)(packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
    Debug("OSPF header    :\n");
    Debug("    version    :   %d\n",ospf_hdr->version);
    Debug("    type       :   %d\n",ospf_hdr->type);
    Debug("    length     :   %d\n",ntohs(ospf_hdr->len));
    Debug("    router id  :   ");dump_ip(ospf_hdr->rid);Debug("\n");
    Debug("    area id    :   %d\n",ntohl(ospf_hdr->aid));
    Debug("    auth type  :   %d\n",ntohs(ospf_hdr->autype));

    switch(ospf_hdr->type)
    {
    case OSPF_TYPE_HELLO:
        dump_ospf_hello(packet,len);
        break;
    case OSPF_TYPE_LSU:
        dump_ospf_lsu(packet,len);
        break;
    }
}

void dump_dhcp_header(const uint8_t * packet, unsigned int len)
{
    struct dhcp_header * dhcp =
        ((struct dhcp_header *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct udp_header)));
    Debug("DHCP header    :\n");
    Debug("    op         :   %d\n", dhcp->op);
    Debug("    htype      :   %d\n", dhcp->htype);
    Debug("    hlen       :   %d\n", dhcp->hlen);
    Debug("    hops       :   %d\n", dhcp->hops);
    Debug("    xid        :   0x%08x\n", ntohl(dhcp->xid));
    Debug("    secs       :   %d\n", ntohs(dhcp->secs));
    Debug("    flags      :   0x%04x\n", ntohs(dhcp->flags));
    Debug("    c_ip       :   "); dump_ip(dhcp->c_ip);Debug("\n");
    Debug("    y_ip       :   "); dump_ip(dhcp->y_ip);Debug("\n");
    Debug("    s_ip       :   "); dump_ip(dhcp->s_ip);Debug("\n");
    Debug("    g_ip       :   "); dump_ip(dhcp->g_ip);Debug("\n");
    Debug("    c_mac      :   "); dump_mac(dhcp->c_mac);Debug("\n");
    Debug("    s_name     :   %s\n", dhcp->s_name);
    Debug("    file       :   %s\n", dhcp->file);
    Debug("    magic      :   0x%08x\n", dhcp->magic); 
}

void dump_udp_hdr(const uint8_t * packet, unsigned int len)
{
    struct udp_header * udp = ((struct udp_header *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)));
    Debug("UDP header     :\n");
    Debug("    src port   :   %d\n", ntohs(udp->src_port));
    Debug("    dst port   :   %d\n", ntohs(udp->dst_port));
    Debug("    len        :   %d\n", ntohs(udp->len));
    Debug("    cksum      :   0x%x\n", ntohs(udp->cksum));

    if(ntohs(udp->dst_port) == 67 || ntohs(udp->dst_port) == 68)
    {
        dump_dhcp_header(packet,len);
    }
}

void dump_ip_hdr(const uint8_t * packet, unsigned int len)
{
    struct ip * ip_hdr = (struct ip *) (packet + sizeof(struct sr_ethernet_hdr));
    Debug("IP header      :\n");
    Debug("    hdr len    :   %d\n",ip_hdr->ip_hl);
    Debug("    version    :   %d\n",ip_hdr->ip_v);
    Debug("    tos        :   %d\n",ip_hdr->ip_tos);
    Debug("    tot len    :   %d\n",ntohs(ip_hdr->ip_len));
    Debug("    id         :   %d\n",ntohs(ip_hdr->ip_id));
    Debug("    fragment of:   %04x\n",ntohs(ip_hdr->ip_off));
    Debug("    ttl        :   %d\n",ip_hdr->ip_ttl);
    Debug("    protocol   :   %02d\n",ip_hdr->ip_p);
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
    case IP_P_UDP:
        dump_udp_hdr(packet,len);
        break;
    case IP_P_OSPF:
        dump_ospf_hdr(packet,len);
        break;        
    }
}

int __debug_validate(struct sr_options * opt, const uint8_t * packet)
{
    int eth_type;
    int ip_type;
    int ospf_type;
    if(opt->verbose) return 1;
    eth_type = ntohs(B_ETH_HDR(packet)->ether_type);
    if(eth_type == ETHERTYPE_ARP) return opt->show_arp;
    else if(eth_type == ETHERTYPE_IP)
    {
        if(opt->show_ip) return 1;
        ip_type = B_IP_HDR(packet)->ip_p;
        if(ip_type == IP_P_ICMP) return opt->show_icmp;
        else if(ip_type == IP_P_OSPF)
        {
            if(opt->show_ospf) return 1;
            ospf_type = B_OSPF_HDR(packet)->type;
            if(ospf_type == OSPF_TYPE_HELLO) return opt->show_ospf_hello;
            else if(ospf_type == OSPF_TYPE_LSU) return opt->show_ospf_lsu;
        }
        else if(ip_type == IP_P_TCP) return opt->show_tcp;
        else if(ip_type == IP_P_UDP)
        {
            if(opt->show_udp) return 1;
            if(ntohs(B_UDP_HDR(packet)->dst_port) == 67 || ntohs(B_UDP_HDR(packet)->dst_port) == 68)
                return opt->show_dhcp;
        }
    }
    return 0;  
}

#include <sys/time.h>
void dump_raw(struct sr_options * opt, const uint8_t * packet, unsigned int len, const char * message)
{
#ifdef _DEBUG_
    if(__debug_validate(opt,packet))
    {
        if(message)
        {
            printf("\n%s\n",message);
        }
        struct sr_ethernet_hdr * eth_hdr = (struct sr_ethernet_hdr *)packet;
        struct timeval tv;
        gettimeofday(&tv,0);
        printf("\n%ld:%ld\n", tv.tv_sec, tv.tv_usec);

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
#endif
}

