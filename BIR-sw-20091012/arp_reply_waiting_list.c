#include <stdlib.h>
#include <string.h>
#include "router.h"
#include "lwtcp/lwip/sys.h"
#include "debug.h"

void * arwl_key_get(void * data)
{
    return &((struct arwl_entry *)data)->next_hop;
}

void __delete_arwl(void * data)
{
    struct arwl_entry * entry = (struct arwl_entry *) data;
    struct arwl_list_entry * l_entry;    
    while((l_entry = ((struct arwl_list_entry * )fifo_pop(entry->packet_list))))
    {
        router_free_packet(l_entry->packet);
        free(l_entry);
    }
    free(entry->packet_list);
    free(entry);
}


void arp_reply_waiting_list_alert_host_unreachable(struct arwl_entry * entry)
{
    struct arwl_list_entry * pl_entry;
    while((pl_entry = fifo_pop(entry->packet_list)))
    {
        dump_ip(entry->next_hop);
        icmp_send_host_unreachable(pl_entry->packet);
    }
}


int arp_handler_incr(void * data, void * user_data)
{
    struct arwl_entry * entry = (struct arwl_entry *) data;
    struct fifo * delete_list = (struct fifo *)user_data;

    if(entry->request_ttl == 0)
    {
        if(entry->request_number >= ARP_REQUEST_MAX)
        {
            Debug("\n\nARP timeout ");dump_ip(entry->next_hop);Debug("\n\n");
            fifo_push(delete_list,entry);            
        }
        else
        {
            arp_request(entry->sr,entry->next_hop, entry->thru_interface);
            entry->request_ttl = ARP_REQUEST_TIMEOUT;
            entry->request_number += 1;
        }
    }
    else
    {
        entry->request_ttl -= 1;
    }
    
    return 0;
}

void arp_request_handler_thread(void * arg)
{
    struct sr_instance * sr = (struct sr_instance *)arg;
    struct arp_reply_waiting_list * list = ARWL(sr);
    struct fifo * delete_list = fifo_create();
    struct arwl_entry * entry;

    while(1)
    {
        usleep(1000000);

        if(ROUTER(sr)->ready)
        {
            if(list->exit_signal == 0)
            {
                list->exit_signal = 1;
                fifo_destroy(delete_list);
                return;
            }
        
            mutex_lock(list->mutex);
            assoc_array_walk_array(list->array,arp_handler_incr,delete_list);
        
            while((entry = fifo_pop(delete_list)))
            {
                arp_reply_waiting_list_alert_host_unreachable(entry);
                __delete_arwl(assoc_array_delete(list->array,&entry->next_hop));
            }
            mutex_unlock(list->mutex);
        }
    }
}

void arp_reply_waiting_list_create(struct sr_instance * sr)
{
    NEW_STRUCT(ret,arp_reply_waiting_list);
    ROUTER(sr)->arwl = ret;
    ret->array = assoc_array_create(arwl_key_get,assoc_array_key_comp_int);
    ret->exit_signal = 1;
    ret->mutex = mutex_create();
    
    sys_thread_new(arp_request_handler_thread,sr);
}

void arp_reply_waiting_list_destroy(struct arp_reply_waiting_list * list)
{
    list->exit_signal = 0;
    while(list->exit_signal == 0)
    {
    }
    assoc_array_delete_array(list->array,__delete_arwl);
    mutex_destroy(list->mutex);
    free(list);       
}

void arp_reply_waiting_list_add(struct arp_reply_waiting_list * list, struct sr_packet * packet,
                                uint32_t next_hop, const char * thru_interface)
{
    struct arwl_entry * entry;
    NEW_STRUCT(l_entry,arwl_list_entry);
    mutex_lock(list->mutex);

    l_entry->packet = router_copy_packet(packet);
    l_entry->next_hop = next_hop;

    entry = (struct arwl_entry *)assoc_array_read(list->array,&next_hop);

    if(entry == NULL)
    {
        entry = (struct arwl_entry *)malloc(sizeof(struct arwl_entry));
        entry->sr = packet->sr;
        entry->next_hop = next_hop;
        entry->packet_list = fifo_create();
        entry->request_number = 0;
        entry->request_ttl = ARP_REQUEST_TIMEOUT;
        assoc_array_insert(list->array,entry);
    }

    memcpy(entry->thru_interface, thru_interface, ETHER_ADDR_LEN);

    fifo_push(entry->packet_list,l_entry);
    
    mutex_unlock(list->mutex);
}

void arp_reply_waiting_list_dispatch(struct arp_reply_waiting_list * list, uint32_t ip)
{
    struct arwl_entry * entry;
    struct arwl_list_entry * l_entry;
    mutex_lock(list->mutex);
    entry = (struct arwl_entry *)assoc_array_delete(list->array,&ip);
    
    if(entry)
    {
        while((l_entry = ((struct arwl_list_entry * )fifo_pop(entry->packet_list))))
        {
            ip_forward_packet(l_entry->packet,l_entry->next_hop,entry->thru_interface);
        }
        __delete_arwl(entry);
    }
    mutex_unlock(list->mutex);
}


void arp_request_handler_process_reply(struct sr_packet * packet)
{
    struct sr_arphdr * arp_hdr = ARP_HDR(packet);
    arp_cache_add(packet->sr,arp_hdr->ar_sip,arp_hdr->ar_sha);
    arp_reply_waiting_list_dispatch(ROUTER(packet->sr)->arwl, ARP_HDR(packet)->ar_sip);
}


void arp_request_handler_make_request(struct sr_packet * packet, uint32_t next_hop, char * thru_interface)
{
    if(assoc_array_read(ROUTER(packet->sr)->arwl->array,&next_hop) == NULL)
    {
        arp_request(packet->sr,next_hop,thru_interface);
    }
    arp_reply_waiting_list_add(ROUTER(packet->sr)->arwl, packet,next_hop,thru_interface);
}
