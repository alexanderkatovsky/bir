#ifndef GRIZLY_H
#define GRIZLY_H

#include <sys/types.h>
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include <stdlib.h>
#include <string.h>


void grizly_dump_packet(uint8_t * packet, unsigned int len);
void process_arp_data(struct sr_instance * inst, struct sr_arphdr * request, char * interface);

unsigned short ipheader_checksum(const struct ip * ip_hdr);
unsigned short icmpheader_checksum(const uint8_t * icmp_hdr, unsigned int len);

void dump_raw(uint8_t * packet, unsigned int len);
void dump_ip(uint32_t ip);
void dump_mac(uint8_t * mac);

void add_to_arp_cache(uint32_t ip, unsigned char * ha);
int get_MAC_from_arp_cache(uint32_t ip, unsigned char * ha);
void init_arp_cache();
void clear_arp_cache();

int get_MAC_and_IP_from_interface_name(struct sr_instance * inst, char * interface, unsigned char * ha, uint32_t * ip);
int get_MAC_from_interface_name(struct sr_instance * inst, char * interface, unsigned char * ha);

void add_to_wfar_list(struct sr_instance * inst,uint8_t * packet, unsigned int len, uint32_t next_hop, char* interface);
void process_wfar_list_on_arp_reply(uint32_t ip);
void init_wfar_list();
void clear_wfar_list();

void forward_packet(struct sr_instance * sr,uint8_t * packet, unsigned int len, uint32_t next_hop, char* interface);

#endif


