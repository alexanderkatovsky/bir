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
void grizly_process_arp_request(struct sr_instance * inst, struct sr_arphdr * request, char * interface);

unsigned short ipheader_checksum(const struct ip * ip_hdr);
unsigned short icmpheader_checksum(const uint8_t * icmp_hdr, unsigned int len);

#endif


