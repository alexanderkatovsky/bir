#include "nat.h"
#include <unistd.h>
#include "router.h"
#include "reg_defines.h"


void nat_hw_insert_entry(struct sr_instance * sr, struct nat_entry * entry, uint8_t * src_MAC, uint8_t * dst_MAC)
{
#ifdef _CPUMODE_
    struct nf2device * device = &ROUTER(sr)->device;
    struct nat_entry_ip_port ipp;
    struct nat_entry * entry2;
    unsigned int port;
    uint32_t mac_hi,mac_lo;

    writeReg(device, NAT_TABLE_RD_ADDR, NAT(sr)->hw_i);
    readReg(device, NAT_TABLE_IP_HOST, &ipp.ip);
    readReg(device, NAT_TABLE_PORT_HOST, &port);
    ipp.port = port;
    ipp.ip = htonl(ipp.ip);

    if((entry2 = bi_assoc_array_read_1(NAT(sr)->table, &ipp)) != NULL)
    {
        entry2->hw_i = -1;
    }

    writeReg(device, NAT_TABLE_IP_NAT_OUT, ntohl(entry->outbound.out.ip));
    writeReg(device, NAT_TABLE_PORT_NAT_OUT, entry->outbound.out.port);
    writeReg(device, NAT_TABLE_IP_DST, ntohl(entry->outbound.dst.ip));
    writeReg(device, NAT_TABLE_PORT_DST, entry->outbound.dst.port);
    writeReg(device, NAT_TABLE_IP_HOST, ntohl(entry->inbound.ip));
    writeReg(device, NAT_TABLE_PORT_HOST, entry->inbound.port);
    writeReg(device, NAT_TABLE_IF_NAT_IN, entry->in_iface);
    writeReg(device, NAT_TABLE_IF_NAT_OUT, entry->out_iface);

    mac_lo = ntohl(*((uint32_t *)(src_MAC+2)));
    mac_hi = ntohl(*((uint32_t*)(src_MAC))) >> 16;
    writeReg(device, NAT_TABLE_ENTRY_MAC_HOST_LO, mac_lo);
    writeReg(device, NAT_TABLE_ENTRY_MAC_HOST_HI, mac_hi);        

    mac_lo = ntohl(*((uint32_t *)(dst_MAC+2)));
    mac_hi = ntohl(*((uint32_t*)(dst_MAC))) >> 16;
    writeReg(device, NAT_TABLE_ENTRY_MAC_NEXT_HOP_LO, mac_lo);
    writeReg(device, NAT_TABLE_ENTRY_MAC_NEXT_HOP_HI, mac_hi);
    
    writeReg(device, NAT_TABLE_WR_ADDR, NAT(sr)->hw_i);
    entry->hw_i = NAT(sr)->hw_i;

    NAT(sr)->hw_i += 1;
    NAT(sr)->hw_i %= NAT_TABLE_DEPTH;
#endif
}

void nat_hw_delete_entry(struct sr_instance * sr, int i)
{
#ifdef _CPUMODE_
    struct nf2device * device = &ROUTER(sr)->device;

    if(i != -1)
    {        
        writeReg(device, NAT_TABLE_IP_NAT_OUT, 0);
        writeReg(device, NAT_TABLE_PORT_NAT_OUT, 0);
        writeReg(device, NAT_TABLE_IP_DST, 0);
        writeReg(device, NAT_TABLE_PORT_DST, 0);
        writeReg(device, NAT_TABLE_IP_HOST, 0);
        writeReg(device, NAT_TABLE_PORT_HOST, 0);
        writeReg(device, NAT_TABLE_IF_NAT_IN, 0);
        writeReg(device, NAT_TABLE_IF_NAT_OUT, 0);
        writeReg(device, NAT_TABLE_ENTRY_COUNTER, 0);
        writeReg(device, NAT_TABLE_ENTRY_MAC_NEXT_HOP_HI, 0);
        writeReg(device, NAT_TABLE_ENTRY_MAC_NEXT_HOP_LO, 0);
        writeReg(device, NAT_TABLE_ENTRY_MAC_HOST_HI, 0);
        writeReg(device, NAT_TABLE_ENTRY_MAC_HOST_LO, 0);        
        writeReg(device, NAT_TABLE_WR_ADDR, i);
    }
#endif
}

void nat_hw_init(struct sr_instance * sr)
{
#ifdef _CPUMODE_
    int i;
    for(i = 0; i < NAT_TABLE_DEPTH; i++)
    {
        nat_hw_delete_entry(sr, i);
    }
#endif
}

struct nat_depricate_entries_s
{
    struct fifo * delete;
    struct sr_instance * sr;
};

int nat_depricate_entries_a(void * data, void * userdata)
{
    struct nat_entry * entry = (struct nat_entry *)data;
    struct nat_depricate_entries_s * es = (struct nat_depricate_entries_s *)userdata;

#ifdef _CPUMODE_ 
    uint32_t counter;
    struct nf2device * device = &ROUTER(es->sr)->device;
    if(entry->hw_i >= 0)
    {
        writeReg(device, NAT_TABLE_RD_ADDR, entry->hw_i);
        readReg(device, NAT_TABLE_ENTRY_COUNTER, &counter);
        if(counter > entry->entry_counter)
        {
            entry->ttl = NAT_TTL;
            entry->entry_counter = counter;
        }
    }
#endif
    entry->ttl -= 1;

    if(entry->ttl == 0)
    {
        fifo_push(es->delete, entry);
    }
    
    return 0;
}

void nat_depricate_entries(struct sr_instance * sr)
{
    struct fifo * delete = fifo_create();
    struct nat_entry * entry;
    struct nat_depricate_entries_s es = {delete,sr};

    bi_assoc_array_walk_array(NAT(sr)->table, nat_depricate_entries_a,  &es);

    while((entry = fifo_pop(delete)))
    {
        bi_assoc_array_delete_1(NAT(sr)->table, &entry->inbound);
        nat_hw_delete_entry(sr,entry->hw_i);
        free(entry);
    }
    
    fifo_destroy(delete);
}

void nat_thread_run(struct sr_instance * sr)
{
    mutex_lock(NAT(sr)->mutex);
    nat_depricate_entries(sr);
    mutex_unlock(NAT(sr)->mutex);
}

void * nat_get_INBOUND(void * data)
{
    return &((struct nat_entry *)data)->inbound;
}

void * nat_get_OUTBOUND(void * data)
{
    return &((struct nat_entry *)data)->outbound;
}

int nat_entry_ip_port_cmp(void * d1, void * d2)
{
    struct nat_entry_ip_port * e1 = (struct nat_entry_ip_port *)d1;
    struct nat_entry_ip_port * e2 = (struct nat_entry_ip_port *)d2;
    
    if(e1->ip < e2->ip) return ASSOC_ARRAY_KEY_LT;
    else if(e1->ip > e2->ip) return ASSOC_ARRAY_KEY_GT;
    else
    {
        if(e1->port < e2->port) return ASSOC_ARRAY_KEY_LT;
        else if(e1->port > e2->port) return ASSOC_ARRAY_KEY_GT;
        else return ASSOC_ARRAY_KEY_EQ;
    }
}

int nat_entry_OUTBOUND_cmp(void * d1, void * d2)
{
    struct nat_entry_OUTBOUND * e1 = (struct nat_entry_OUTBOUND *)d1;
    struct nat_entry_OUTBOUND * e2 = (struct nat_entry_OUTBOUND *)d2;

    int cmp = nat_entry_ip_port_cmp(&e1->out,&e2->out);

    if(cmp == ASSOC_ARRAY_KEY_LT) return ASSOC_ARRAY_KEY_LT;
    else if(cmp == ASSOC_ARRAY_KEY_GT) return ASSOC_ARRAY_KEY_GT;
    else
    {
        cmp = nat_entry_ip_port_cmp(&e1->dst,&e2->dst);
        if(cmp == ASSOC_ARRAY_KEY_LT) return ASSOC_ARRAY_KEY_LT;
        else if(cmp == ASSOC_ARRAY_KEY_GT) return ASSOC_ARRAY_KEY_GT;        
        else return ASSOC_ARRAY_KEY_EQ;
    }    
}


void nat_create(struct sr_instance * sr)
{
    NEW_STRUCT(ret,nat_table);
    ROUTER(sr)->nat = ret;
    ret->table = bi_assoc_array_create(nat_get_INBOUND,nat_entry_ip_port_cmp,nat_get_OUTBOUND,nat_entry_OUTBOUND_cmp);
    
    ret->mutex = mutex_create();
    nat_hw_init(sr);
    ret->hw_i = 0;
    router_add_thread(sr,nat_thread_run, NULL);
}

void nat_destroy(struct nat_table * nat)
{
    bi_assoc_array_delete_array(nat->table, assoc_array_delete_self);
    mutex_destroy(nat->mutex);
    free(nat);
}


int nat_port_alloc(struct sr_instance * sr, struct nat_entry_OUTBOUND * outbound)
{
    struct bi_assoc_array * table = NAT(sr)->table;
    int i;
    for(i = 1; i < 0xffff; i++)
    {
        outbound->out.port = i;
        if(bi_assoc_array_read_2(table, outbound) == NULL)
        {
            return 1;
        }
    }
    return 0;
}

int nat_out(struct sr_instance * sr, uint32_t * src_ip, uint16_t * src_port,
            uint32_t dst_ip, uint16_t dst_port, char * out_iface, char * in_iface, uint8_t * src_MAC, uint32_t next_hop)
{
    struct nat_entry_ip_port src = {*src_ip, *src_port};
    struct nat_entry * entry;
    int ret = 0;
#ifdef _CPUMODE_
    uint8_t dst_MAC[ETHER_ADDR_LEN];
    int hww = arp_cache_get_MAC_from_ip(ARP_CACHE(sr), next_hop, dst_MAC);
#endif    

    mutex_lock(NAT(sr)->mutex);

    entry = bi_assoc_array_read_1(NAT(sr)->table, &src);
    interface_list_get_MAC_and_IP_from_name(INTERFACE_LIST(sr), out_iface, NULL, src_ip);

    if(entry)
    {
        ret = 1;
        if(dst_ip == entry->outbound.dst.ip && dst_port == entry->outbound.dst.port &&
           *src_ip == entry->outbound.out.ip)
        {
            *src_port = entry->outbound.out.port;
        }
        else
        {
            /* keep the old port number  */
            bi_assoc_array_delete_1(NAT(sr)->table, &src);
            entry->outbound.out.ip = *src_ip;
            entry->outbound.dst.ip = dst_ip;
            entry->outbound.dst.port = dst_port; 
            bi_assoc_array_insert(NAT(sr)->table, entry);
        }
#ifdef _CPUMODE_
        if(hww)
        {
            nat_hw_delete_entry(sr,entry->hw_i);
            entry->hw_i = -1;
            nat_hw_insert_entry(sr,entry,src_MAC,dst_MAC);
        }
#endif        
    }
    else
    {
        entry = (struct nat_entry *)malloc(sizeof(struct nat_entry));
        entry->inbound = src;
        entry->outbound.out.ip = *src_ip;
        entry->outbound.dst.ip = dst_ip;
        entry->outbound.dst.port = dst_port;
        entry->entry_counter = 0;
        entry->hw_i = -1;

        entry->in_iface = interface_list_get_output_port(sr,in_iface);
        entry->out_iface = interface_list_get_output_port(sr,out_iface);

        if(nat_port_alloc(sr,&entry->outbound))
        {
            ret = 1;
            *src_port = entry->outbound.out.port;
            bi_assoc_array_insert(NAT(sr)->table, entry);
#ifdef _CPUMODE_
            if(hww)
            {
                nat_hw_insert_entry(sr,entry,src_MAC,dst_MAC);
            }
#endif            
        }
        else
        {
            free(entry);
        }
    }

    if(ret)
    {
        entry->ttl = NAT_TTL;
    }

    mutex_unlock(NAT(sr)->mutex);

    return ret;
}

int nat_in(struct sr_instance * sr, uint32_t src_ip, uint16_t src_port, uint32_t * dst_ip, uint16_t * dst_port)
{
    struct nat_entry_ip_port out = {*dst_ip, *dst_port};
    struct nat_entry_ip_port dst = {src_ip, src_port};
    struct nat_entry_OUTBOUND outbound = {out, dst};
    struct nat_entry * entry = bi_assoc_array_read_2(NAT(sr)->table, &outbound);

    if(entry)
    {
        *dst_ip = entry->inbound.ip;
        *dst_port = entry->inbound.port;
        return 1;
    }
    return 0;
}

int __nat_show_a(void * data, void * userdata)
{
    print_t print = (print_t)userdata;
    struct nat_entry * entry = (struct nat_entry *)data;

    print_ip_port(entry->inbound.ip,entry->inbound.port,print);
    print_ip_port(entry->outbound.out.ip,entry->outbound.out.port,print);
    print_ip_port(entry->outbound.dst.ip,entry->outbound.dst.port,print);
    print(" (ttl: %d)\n", entry->ttl);
    return 0;
}

void nat_show(struct sr_instance * sr, print_t print)
{
    print("\nNAT table:\n");
    bi_assoc_array_walk_array(NAT(sr)->table,__nat_show_a,print);
}
