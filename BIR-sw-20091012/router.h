#ifndef ROUTER_H
#define ROUTER_H

#include "interface_list.h"
#include "forwarding_table.h"
#include "eth_headers.h"
#include "sr_rt.h"
#include "arp_cache.h"
#include "arp_reply_waiting_list.h"

struct sr_packet
{
    struct sr_instance* sr;
    uint8_t * packet;
    unsigned int len;
    char * interface;
};

struct sr_router
{
    struct interface_list * iflist;
    struct forwarding_table * fwd_table;
    struct arp_cache * a_cache;
    struct arp_reply_waiting_list * arwl;
};

struct sr_router * router_init();
void router_handle_incoming_packet(struct sr_packet * packet);
struct sr_packet * router_construct_packet(struct sr_instance * sr, const uint8_t * packet, unsigned int len, const char* interface);
void router_load_static_routes(struct sr_instance * sr);
void router_free_packet(struct sr_packet * );

void router_swap_eth_header_and_send(struct sr_packet * packet);

#define ROUTER(sr) ((struct sr_router*)((sr)->router))

#define ETH_HDR(packet) ((struct sr_ethernet_hdr *) ((packet)->packet))
#define ARP_HDR(packet) ((struct sr_arphdr *) ((packet)->packet + sizeof(struct sr_ethernet_hdr)))
#define IP_HDR(packet)  ((struct ip *)((packet)->packet + sizeof(struct sr_ethernet_hdr)))
#define ICMP_HDR(packet) ((struct icmphdr *)((packet)->packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)))

void arp_handle_incoming_packet(struct sr_packet * packet);
void arp_request(const struct sr_packet * packet, uint32_t ip, const char * interface);

void ip_handle_incoming_packet(struct sr_packet * packet);
void ip_forward_packet(struct sr_packet * packet, uint32_t next_hop, const char * thru_interface);

unsigned short checksum_ipheader(const struct ip * ip_hdr);
unsigned short checksum_icmpheader(const uint8_t * icmp_hdr, unsigned int len);

void icmp_handle_incoming_packet(struct sr_packet * packet);
void icmp_send_prot_unreachable(struct sr_packet * packet);
void icmp_send_time_exceeded(struct sr_packet * packet);
void icmp_send_no_route(struct sr_packet * packet);

#endif
