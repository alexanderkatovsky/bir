#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define NEW_STRUCT(varname,structname)                              \
    struct structname * varname =                                   \
        (struct structname * )malloc(sizeof(struct structname));

struct ip_address
{
    uint32_t subnet;
    uint32_t mask;
};

int ip_address_cmp(void * k1, void * k2);

struct sr_options
{
    /* arp proxy (respond to arp request if the destination address is in the forwarding table) */
    int arp_proxy;
    int disable_ospf;
    int aid;
    int RCPPort;
    /* inbound nat interfaces */
    struct assoc_array * inbound;
    /* outbound nat interfaces */
    struct assoc_array * outbound;
    struct assoc_array * ospf_disabled_interfaces;
    struct assoc_array * dhcp;

    int verbose;
    int show_arp;
    int show_ip;
    int show_ospf;
    int show_ospf_hello;
    int show_ospf_lsu;
    int show_icmp;
    int show_tcp;
    int show_udp;
    int show_dhcp;
};

void sr_router_default_options(struct sr_options * opt);

#endif


