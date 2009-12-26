#ifndef __NAT_H
#define __NAT_H

#include "bi_assoc_array.h"
#include "common.h"
#include "mutex.h"
#include "debug.h"

struct sr_instance;

#define NAT_TTL 60

struct nat_entry_ip_port
{
    uint32_t ip;
    uint16_t port;
};

struct nat_entry_OUTBOUND
{
    struct nat_entry_ip_port out;
    struct nat_entry_ip_port dst;
};

struct nat_entry
{
    struct nat_entry_ip_port inbound;
    struct nat_entry_OUTBOUND outbound;

    uint32_t in_iface;
    uint32_t out_iface;
    
    int ttl;
    int hw_i;
};

struct nat_table
{
    /* (src_ip, src_port, out_ip, out_port, dst_ip, dst_port)  */
    struct bi_assoc_array * table;

    /*the next entry to write to in the NAT hardware table*/
    int hw_i;

    struct sr_mutex * mutex;
};

void nat_destroy(struct nat_table * nat);
void nat_create(struct sr_instance * sr);
int nat_out(struct sr_instance * sr, uint32_t * src_ip, uint16_t * src_port, uint32_t dst_ip,
            uint16_t dst_port, char * out_iface, char * in_iface);
int nat_in(struct sr_instance * sr, uint32_t src_ip, uint16_t src_port, uint32_t * dst_ip, uint16_t * dst_port);

void nat_show(struct sr_instance * sr, print_t print);

#endif
