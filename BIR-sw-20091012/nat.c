#include "nat.h"
#include <unistd.h>
#include "router.h"
#include "lwtcp/lwip/sys.h"

void nat_free_port(struct sr_instance * sr, int port)
{
    int * i = assoc_array_read(NAT(sr)->used_ports, &port);
    if(i)
    {
        free(i);
    }
}

int nat_depricate_entries_a(void * data, void * userdata)
{
    struct nat_entry * entry = (struct nat_entry *)data;
    struct fifo * delete = (struct fifo *)userdata;

    entry->ttl -= 1;

    if(entry->ttl == 0)
    {
        fifo_push(delete, entry);
    }
    
    return 0;
}

void nat_depricate_entries(struct sr_instance * sr)
{
    struct fifo * delete = fifo_create();
    struct nat_entry * entry;

    bi_assoc_array_walk_array(NAT(sr)->table, nat_depricate_entries_a,  delete);

    while((entry = fifo_pop(delete)))
    {
/*        printf("deprecating entry %x\n",entry->outbound.out.port);*/
        bi_assoc_array_delete_1(NAT(sr)->table, &entry->inbound);
        nat_free_port(sr, entry->outbound.out.port);
        free(entry);
    }
    
    fifo_destroy(delete);
}

void nat_thread(void * data)
{
    struct sr_instance * sr = (struct sr_instance *)data;

    while(1)
    {
        if(ROUTER(sr)->ready)
        {
            mutex_lock(NAT(sr)->mutex);

            if(NAT(sr)->exit_signal == 0)
            {
                NAT(sr)->exit_signal = 1;
                return;
            }

            nat_depricate_entries(sr);
            
            mutex_unlock(NAT(sr)->mutex);
        }
        
        usleep(1000000);
    }    
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
    ret->used_ports = assoc_array_create(assoc_array_get_self, assoc_array_key_comp_int);
    
    ret->exit_signal = 1;
    ret->mutex = mutex_create();
    sys_thread_new(nat_thread,sr);
}

void nat_destroy(struct nat_table * nat)
{
    nat->exit_signal = 0;
    while(nat->exit_signal == 0)
    {
        usleep(10000);
    }
    bi_assoc_array_delete_array(nat->table, assoc_array_delete_self);
    assoc_array_delete_array(nat->used_ports, assoc_array_delete_self);
    mutex_destroy(nat->mutex);
    free(nat);
}


/*
 * A more efficient approach: keep an array of freed entries.  If this is non-empty then
 * use one of these, else use 1 + maximum entry in the used array (a function returning 1
 * in assoc_array_walk will get the max entry)
 * */
int nat_port_alloc(struct sr_instance * sr, uint16_t * port)
{
    struct assoc_array * used_ports = NAT(sr)->used_ports;
    int i;
    int * pi;
    for(i = 1; i < 0xffff; i++)
    {
        if(assoc_array_read(used_ports, &i) == NULL)
        {
            pi = malloc(sizeof(int));
            *pi = i;
            *port = i;
            assoc_array_insert(used_ports, pi);
            return 1;
        }
    }
    return 0;
}

int nat_out(struct sr_instance * sr, uint32_t * src_ip, uint16_t * src_port,
            uint32_t dst_ip, uint16_t dst_port, char * out_iface)
{
    struct nat_entry_ip_port src = {*src_ip, *src_port};
    struct nat_entry * entry;
    int ret = 1;

    mutex_lock(NAT(sr)->mutex);

    entry = bi_assoc_array_read_1(NAT(sr)->table, &src);
    interface_list_get_MAC_and_IP_from_name(INTERFACE_LIST(sr), out_iface, NULL, src_ip);

    if(entry)
    {
        if(dst_ip == entry->outbound.dst.ip && dst_port == entry->outbound.dst.port &&
           *src_ip == entry->outbound.out.ip)
        {
            *src_port = entry->outbound.out.port;
        }
        else
        {
            bi_assoc_array_delete_1(NAT(sr)->table, &src);
            entry->outbound.out.ip = *src_ip;
            entry->outbound.dst.ip = dst_ip;
            entry->outbound.dst.port = dst_port; /* keep the old port number  */
            bi_assoc_array_insert(NAT(sr)->table, entry);
        }
    }
    else if(nat_port_alloc(sr, src_port))
    {
        entry = (struct nat_entry *)malloc(sizeof(struct nat_entry));
        entry->inbound = src;
        entry->outbound.out.ip = *src_ip;
        entry->outbound.out.port = *src_port;
        entry->outbound.dst.ip = dst_ip;
        entry->outbound.dst.port = dst_port; 
        bi_assoc_array_insert(NAT(sr)->table, entry);
    }
    else
    {
        ret = 0;
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
    else
    {
        return 0;
    }
}
