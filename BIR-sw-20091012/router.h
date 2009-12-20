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
#include "link_state_graph.h"
#include "nat.h"
#include "nf2util.h"
#include "RCP.h"

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
    struct link_state_graph * lsg;
    struct nat_table * nat;

    char default_interface[SR_NAMELEN];

    uint32_t rid;
    uint32_t ospf_seq;

    int ready;

    struct sr_options opt;
    struct RCPServer * rcp_server;

#ifdef _CPUMODE_
    struct nf2device device;
#endif
};


void router_create(struct sr_instance * sr, struct sr_options * opt);
void router_destroy(struct sr_router * router);
void router_handle_incoming_packet(struct sr_packet * packet);
struct sr_packet * router_construct_packet(struct sr_instance * sr, const uint8_t * packet,
                                           unsigned int len, const char* interface);
struct sr_packet * router_copy_packet(struct sr_packet * packet);
void router_load_static_routes(struct sr_instance * sr);
void router_free_packet(struct sr_packet * );

void router_swap_eth_header_and_send(struct sr_packet * packet);
void router_add_interface(struct sr_instance * sr, struct sr_vns_if * interface);

#define ROUTER(sr) ((struct sr_router*)((sr)->router))
#define INTERFACE_LIST(sr) ((struct interface_list *)(ROUTER(sr)->iflist))
#define FORWARDING_TABLE(sr) ((struct forwarding_table *)(ROUTER(sr)->fwd_table))
#define ARP_CACHE(sr) ((struct arp_cache *)(ROUTER(sr)->a_cache))
#define ARWL(sr) ((struct arp_reply_waiting_list *)(ROUTER(sr)->arwl))
#define LSG(sr) ((struct link_state_graph *)(ROUTER(sr)->lsg))
#define OPTIONS(sr) (&(ROUTER(sr)->opt))
#define NAT(sr) (ROUTER(sr)->nat)


#define B_ETH_HDR(packet) ((struct sr_ethernet_hdr *) ((packet)))
#define B_ARP_HDR(packet) ((struct sr_arphdr *) ((packet) + sizeof(struct sr_ethernet_hdr)))
#define B_IP_HDR(packet)  ((struct ip *)((packet) + sizeof(struct sr_ethernet_hdr)))
#define B_ICMP_HDR(packet) ((struct icmphdr *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)))
#define B_OSPF_HDR(packet) ((struct ospfv2_hdr *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)))
#define B_HELLO_HDR(packet) ((struct ospfv2_hello_hdr *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr)))
#define B_LSU_HDR(packet) ((struct ospfv2_lsu_hdr *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr)))
#define B_LSU_START(packet) ((struct ospfv2_lsu *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr)))
#define B_TCP_HDR(packet) ((struct tcpheader *)((packet) + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)))

#define ETH_HDR(packet) B_ETH_HDR((packet)->packet)
#define ARP_HDR(packet) B_ARP_HDR((packet)->packet)
#define IP_HDR(packet)  B_IP_HDR((packet)->packet)
#define ICMP_HDR(packet) B_ICMP_HDR((packet)->packet)
#define OSPF_HDR(packet) B_OSPF_HDR((packet)->packet)
#define HELLO_HDR(packet) B_HELLO_HDR((packet)->packet)
#define LSU_HDR(packet) B_LSU_HDR((packet)->packet)
#define LSU_START(packet) B_LSU_START((packet)->packet)
#define TCP_HDR(packet) B_TCP_HDR((packet)->packet)

void arp_handle_incoming_packet(struct sr_packet * packet);
void arp_request(struct sr_instance * sr, uint32_t ip, char * interface);

void ip_handle_incoming_packet(struct sr_packet * packet);
void ip_send(struct sr_packet * packet, uint32_t next_hop, char * thru_interface);
int ip_send_packet(struct sr_packet * packet);
void ip_forward(struct sr_packet * packet);
void ip_construct_eth_header(uint8_t * packet, const uint8_t * dest_MAC, const uint8_t * src_MAC, uint16_t type);
void ip_construct_ip_header(uint8_t * packet, uint16_t len,
                            uint16_t id, uint8_t ttl,
                            uint8_t p, uint32_t ip_src, uint32_t ip_dest);

unsigned short checksum_ipheader(const struct ip * ip_hdr);
unsigned short checksum_icmpheader(const uint8_t * icmp_hdr, unsigned int len);
unsigned short checksum_ospfheader(const uint8_t * p_ospf_hdr, unsigned int len);
unsigned short cksum(const unsigned short *data, int len);
    
void icmp_handle_incoming_packet(struct sr_packet * packet);
void icmp_send_port_unreachable(struct sr_packet * packet);
void icmp_send_time_exceeded(struct sr_packet * packet);
void icmp_send_host_unreachable(struct sr_packet * packet);
int icmp_send_ping(struct sr_instance * sr, uint32_t ip, uint32_t seq_num, int id, int ttl);

int router_cmp_MAC(void * k1, void * k2);
int router_nat_enabled(struct sr_instance * sr);

void ospf_handle_incoming_packet(struct sr_packet * packet);
void ospf_send_hello(struct sr_instance * sr, struct interface_list_entry * ifentry);
void ospf_construct_ospf_header(uint8_t * packet, uint8_t type, uint16_t len, uint32_t rid, uint32_t aid);
void ospf_construct_lsu_header(uint8_t * packet, uint16_t seq, uint32_t num_adv);

void tcp_handle_incoming_not_for_us(struct sr_packet * packet);
void tcp_handle_incoming_for_us(struct sr_packet * packet);

#endif
