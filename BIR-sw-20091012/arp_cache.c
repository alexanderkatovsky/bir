#include "router.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fifo.h"
#include "debug.h"
#include "reg_defines.h"

struct arp_cache_hw_write_s
{
    int count;
    struct nf2device * device;
};

int arp_cache_hw_write_a(void * data, void * userdata)
{
    struct arp_cache_entry * ace = (struct arp_cache_entry *)data;
    struct arp_cache_hw_write_s * achws = (struct arp_cache_hw_write_s *)userdata;
    uint32_t mac_hi,mac_lo;

    if(achws->count >= ROUTER_OP_LUT_ARP_TABLE_DEPTH)
    {
        return 1;
    }

    mac_lo = ntohl(*((uint32_t *)(ace->MAC+2)));
    mac_hi = ntohl(*((uint32_t*)(ace->MAC))) >> 16;
    
    writeReg(achws->device, ROUTER_OP_LUT_ARP_TABLE_ENTRY_MAC_LO, mac_lo);
    writeReg(achws->device, ROUTER_OP_LUT_ARP_TABLE_ENTRY_MAC_HI, mac_hi);
    writeReg(achws->device, ROUTER_OP_LUT_ARP_TABLE_ENTRY_NEXT_HOP_IP, ntohl(ace->ip));

    writeReg(achws->device, ROUTER_OP_LUT_ARP_TABLE_WR_ADDR, achws->count);
    achws->count += 1;
    
    return 0;
}

void arp_cache_hw_write(struct sr_instance * sr)
{
#ifdef _CPUMODE_
    struct arp_cache_hw_write_s achws= {0,&ROUTER(sr)->device};
    int i;
    mutex_lock(ARP_CACHE(sr)->mutex);
    bi_assoc_array_walk_array(ARP_CACHE(sr)->array, arp_cache_hw_write_a, &achws);
    bi_assoc_array_walk_array(ARP_CACHE(sr)->array_s, arp_cache_hw_write_a, &achws);

    for(i = achws.count; i < ROUTER_OP_LUT_ARP_TABLE_DEPTH; i++)
    {
        writeReg(achws.device, ROUTER_OP_LUT_ARP_TABLE_ENTRY_MAC_LO, 0);
        writeReg(achws.device, ROUTER_OP_LUT_ARP_TABLE_ENTRY_MAC_HI, 0);
        writeReg(achws.device, ROUTER_OP_LUT_ARP_TABLE_ENTRY_NEXT_HOP_IP, 0);
        writeReg(achws.device, ROUTER_OP_LUT_ARP_TABLE_WR_ADDR, i);
    }
    mutex_unlock(ARP_CACHE(sr)->mutex);
#endif
}

int arp_clear_cache(void * data, void * user_data)
{
    struct arp_cache_entry * entry = (struct arp_cache_entry *) data;
    struct fifo * delete_list = (struct fifo *)user_data;

    entry->ttl -= 1;

    if(entry->ttl <= 0)
    {
        fifo_push(delete_list,entry);
    }

    return 0;
}

void arp_cache_thread_run(struct sr_instance * sr)
{
    struct arp_cache * cache = ARP_CACHE(sr);
    struct fifo * delete_list = fifo_create();
    struct arp_cache_entry * entry;
    int hw;
    hw = 0;
        
    mutex_lock(cache->mutex);
    bi_assoc_array_walk_array(cache->array,arp_clear_cache,delete_list);
    while((entry = fifo_pop(delete_list)))
    {
        hw = 1;
        free(bi_assoc_array_delete_1(cache->array,&entry->ip));
    }
    if(hw)
    {
        arp_cache_hw_write(sr);
    }
    fifo_destroy(delete_list);
    mutex_unlock(cache->mutex);

    router_notify(sr, ROUTER_UPDATE_ARP_TABLE);
}

void * arp_cache_get_key(void * data)
{
    return &((struct arp_cache_entry *)data)->ip;
}

void * arp_cache_get_MAC(void * data)
{
    return ((struct arp_cache_entry *)data)->MAC;
}

void arp_cache_create(struct sr_instance * sr)
{
    struct arp_cache * ret = (struct arp_cache *)malloc(sizeof(struct arp_cache));
    ret->array = bi_assoc_array_create(arp_cache_get_key,assoc_array_key_comp_int,
                                       arp_cache_get_MAC,router_cmp_MAC);
    ret->array_s = bi_assoc_array_create(arp_cache_get_key,assoc_array_key_comp_int,
                                       arp_cache_get_MAC,router_cmp_MAC);
    ret->mutex = mutex_create();

    ROUTER(sr)->a_cache = ret;
    router_add_thread(sr,arp_cache_thread_run, NULL);
}

void __delete_arp_cache(void * data)
{
    free((struct arp_cache_entry *)data);
}

void arp_cache_destroy(struct arp_cache * cache)
{
    mutex_destroy(cache->mutex);
    bi_assoc_array_delete_array(cache->array,__delete_arp_cache);
    bi_assoc_array_delete_array(cache->array_s,__delete_arp_cache);    
    free(cache); 
}

int arp_cache_get_MAC_from_ip(struct arp_cache * cache, uint32_t ip, uint8_t * MAC)
{
    struct arp_cache_entry * res;
    int ret = 0;
    mutex_lock(cache->mutex);

    res = bi_assoc_array_read_1(cache->array,&ip);
    if(res)
    {
        memcpy(MAC,res->MAC,ETHER_ADDR_LEN);
        ret = 1;
    }
    else
    {
        res = bi_assoc_array_read_1(cache->array_s,&ip);
        if(res)
        {
            memcpy(MAC,res->MAC,ETHER_ADDR_LEN);
            ret = 1;
        }        
    }

    mutex_unlock(cache->mutex);
    return ret;
}

void arp_cache_add(struct sr_instance * sr, uint32_t ip, const uint8_t * MAC, int isDynamic)
{
    struct bi_assoc_array * cache = isDynamic ? ARP_CACHE(sr)->array : ARP_CACHE(sr)->array_s;
    struct arp_cache_entry * entry;
    mutex_lock(ARP_CACHE(sr)->mutex);
    if((entry = bi_assoc_array_read_2(cache,(void *)MAC)))
    {
        entry->ip = ip;
    }
    else
    {
        entry = (struct arp_cache_entry *)malloc(sizeof(struct arp_cache_entry));
        entry->ip = ip;
        memcpy(entry->MAC,MAC,ETHER_ADDR_LEN);
        bi_assoc_array_insert(cache,entry);
    }
    entry->ttl = ARP_CACHE_TIMEOUT;
    arp_cache_hw_write(sr);
    mutex_unlock(ARP_CACHE(sr)->mutex);

    router_notify(sr, isDynamic ? ROUTER_UPDATE_ARP_TABLE : ROUTER_UPDATE_ARP_TABLE_S);
}

int arp_cache_remove_entry(struct sr_instance * sr, uint32_t ip, int isDynamic)
{
    struct arp_cache_entry * entry;
    struct arp_cache * cache = ARP_CACHE(sr);
    int ret = 0;
    mutex_lock(cache->mutex);
    entry = bi_assoc_array_delete_1(isDynamic ? cache->array : cache->array_s, &ip);
    if(entry)
    {
        ret = 1;
        __delete_arp_cache(entry);
        arp_cache_hw_write(sr);
    }
    mutex_unlock(cache->mutex);
    router_notify(sr, isDynamic ? ROUTER_UPDATE_ARP_TABLE : ROUTER_UPDATE_ARP_TABLE_S);
    return ret;
}

struct arp_cache_loop_i
{
    void (*fn)(uint32_t, uint8_t * MAC, int ttl, void *);
    void * userdata;
};

int arp_cache_loop_a(void * data, void * userdata)
{
    struct arp_cache_entry * e = (struct arp_cache_entry *)data;
    struct arp_cache_loop_i * li = (struct arp_cache_loop_i *)userdata;

    li->fn(e->ip, e->MAC, e->ttl, li->userdata);
    return 0;
}

void arp_cache_loop(struct sr_instance * sr, void (*fn)(uint32_t, uint8_t *, int, void *), void * userdata, int isDynamic)
{
    mutex_lock(ARP_CACHE(sr)->mutex);
    struct arp_cache_loop_i i = {fn,userdata};
    struct bi_assoc_array * array = isDynamic ? ARP_CACHE(sr)->array : ARP_CACHE(sr)->array_s;
    bi_assoc_array_walk_array(array, arp_cache_loop_a, &i);
    mutex_unlock(ARP_CACHE(sr)->mutex);
}

int __arp_cache_purge_a(void * data, void * userdata)
{
    struct arp_cache_entry * entry = (struct arp_cache_entry *)data;
    struct fifo * delete_list = (struct fifo *)userdata;

    fifo_push(delete_list,entry);
    return 0;
}

int arp_cache_purge(struct sr_instance * sr, int isDynamic)
{
    struct arp_cache * cache = ARP_CACHE(sr);
    struct bi_assoc_array * array = isDynamic ? cache->array : cache->array_s;
    struct fifo * delete_list = fifo_create();
    struct arp_cache_entry * entry;
    int i = 0;
    mutex_lock(cache->mutex);
    bi_assoc_array_walk_array(array, __arp_cache_purge_a, delete_list);
    while((entry = fifo_pop(delete_list)))
    {
        i++;
        __delete_arp_cache(bi_assoc_array_delete_1(cache->array,&entry->ip));
    }
    arp_cache_hw_write(sr);
    mutex_unlock(cache->mutex);
    fifo_destroy(delete_list);
    router_notify(sr, isDynamic ? ROUTER_UPDATE_ARP_TABLE : ROUTER_UPDATE_ARP_TABLE_S);
    return i;
}

void arp_cache_alert_packet_received(struct sr_packet * packet)
{
    struct sr_ethernet_hdr * eth_hdr = ETH_HDR(packet);
    struct arp_cache_entry * entry;
    struct arp_cache * cache = ROUTER(packet->sr)->a_cache;
    mutex_lock(cache->mutex);
    entry = bi_assoc_array_read_2(cache->array,eth_hdr->ether_shost);
    if(entry != NULL)
    {
        entry->ttl = ARP_CACHE_TIMEOUT;
        router_notify(packet->sr, ROUTER_UPDATE_ARP_TABLE);
    }
    mutex_unlock(cache->mutex);
}

int arp_cache_print_entry(void * data, void * userdata)
{
    struct arp_cache_entry * entry = (struct arp_cache_entry*)data;
    print_t print = (print_t)userdata;
    
    print_ip(entry->ip,print);
    print("  ");
    print_mac(entry->MAC,print);
    print("  ");
    print("%d",entry->ttl);
    print("\n");
    return 0;
}

void arp_cache_show(struct arp_cache * cache, print_t print)
{
    print("\nARP Cache:\n");
    print("%3s               %3s              %3s\n","ip","MAC","ttl");   
    bi_assoc_array_walk_array(cache->array,arp_cache_print_entry,print);
    print("\n\n");

    print("\nARP Cache (static):\n");
    print("%3s               %3s              %3s\n","ip","MAC","ttl");   
    bi_assoc_array_walk_array(cache->array_s,arp_cache_print_entry,print);
    print("\n\n");
}

