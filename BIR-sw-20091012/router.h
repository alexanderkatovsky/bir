#ifndef ROUTER_H
#define ROUTER_H

#include "interface_list.h"
#include "forwarding_table.h"
#include "eth_headers.h"
#include "sr_rt.h"
#include "arp_cache.h"
#include "arp_reply_waiting_list.h"
#include "pwospf_protocol.h"
#include "common.h"


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

    uint32_t rid;
    uint32_t aid;
};


struct sr_router * router_create(struct sr_instance * sr);
void router_destroy(struct sr_router * router);
void router_handle_incoming_packet(struct sr_packet * packet);
struct sr_packet * router_construct_packet(struct sr_instance * sr, const uint8_t * packet,
                                           unsigned int len, const char* interface);
struct sr_packet * router_copy_packet(struct sr_packet * packet);
void router_load_static_routes(struct sr_instance * sr);
void router_free_packet(struct sr_packet * );

void router_swap_eth_header_and_send(struct sr_packet * packet);

#define ROUTER(sr) ((struct sr_router*)((sr)->router))


#define B_ETH_HDR(packet) ((struct sr_ethernet_hdr *) ((packet)))
#define B_ARP_HDR(packet) ((struct sr_arphdr *) ((packet) + sizeof(struct sr_ethernet_hdr)))
#define B_IP_HDR(packet)  ((struct ip *)((packet) + sizeof(struct sr_ethernet_hdr)))
#define B_ICMP_HDR(packet) ((struct icmphdr *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)))
#define B_OSPF_HDR(packet) ((struct ospfv2_hdr *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)))
#define B_HELLO_HDR(packet) ((struct ospfv2_hello_hdr *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr)))

#define ETH_HDR(packet) B_ETH_HDR((packet)->packet)
#define ARP_HDR(packet) B_ARP_HDR((packet)->packet)
#define IP_HDR(packet)  B_IP_HDR((packet)->packet)
#define ICMP_HDR(packet) B_ICMP_HDR((packet)->packet)
#define OSPF_HDR(packet) B_OSPF_HDR((packet)->packet)
#define HELLO_HDR(packet) B_HELLO_HDR((packet)->packet)

void arp_handle_incoming_packet(struct sr_packet * packet);
void arp_request(struct sr_instance * sr, uint32_t ip, char * interface);

void ip_handle_incoming_packet(struct sr_packet * packet);
void ip_forward_packet(struct sr_packet * packet, uint32_t next_hop, char * thru_interface);

unsigned short checksum_ipheader(const struct ip * ip_hdr);
unsigned short checksum_icmpheader(const uint8_t * icmp_hdr, unsigned int len);
unsigned short checksum_ospfheader(const uint8_t * p_ospf_hdr, unsigned int len);
    
void icmp_handle_incoming_packet(struct sr_packet * packet);
void icmp_send_port_unreachable(struct sr_packet * packet);
void icmp_send_time_exceeded(struct sr_packet * packet);
void icmp_send_host_unreachable(struct sr_packet * packet);

int router_cmp_MAC(void * k1, void * k2);

void ospf_handle_incoming_packet(struct sr_packet * packet);

#endif
