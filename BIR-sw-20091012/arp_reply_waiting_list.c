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
        ip_forward_packet(l_entry->packet,l_entry->next_hop,entry->thru_interface);
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
            fifo_push(delete_list,entry);            
        }
        else
        {
            entry->request_number += 1;
        }
    }
    else
    {
        entry->request_ttl -= 1;
        arp_request(entry->sr,entry->next_hop, entry->thru_interface);
    }
    
    return 0;
}

void arp_request_handler_thread(void * arg)
{
    struct arp_reply_waiting_list * list = (struct arp_reply_waiting_list *)arg;
    struct fifo * delete_list = fifo_create();
    struct arwl_entry * entry;

    while(1)
    {
        usleep(1000000);
        
        if(list->exit_signal == 0)
        {
            list->exit_signal = 1;
            fifo_destroy(delete_list);
            return;
        }
        
        pthread_mutex_lock(&list->mutex);
        assoc_array_walk_array(list->array,arp_handler_incr,delete_list);
        pthread_mutex_unlock(&list->mutex);
        
        while((entry = fifo_pop(delete_list)))
        {
            arp_reply_waiting_list_alert_host_unreachable(entry);
            __delete_arwl(assoc_array_delete(list->array,&entry->next_hop));
        }
    }
}

struct arp_reply_waiting_list * arp_reply_waiting_list_create()
{
    NEW_STRUCT(ret,arp_reply_waiting_list);
    ret->array = assoc_array_create(arwl_key_get,assoc_array_key_comp_int);
    ret->exit_signal = 1;
    pthread_mutex_init(&ret->mutex,NULL);
    
    sys_thread_new(arp_request_handler_thread,ret);
    
    return ret;
}

void arp_reply_waiting_list_destroy(struct arp_reply_waiting_list * list)
{
    pthread_mutex_destroy(&list->mutex);
    list->exit_signal = 0;
    while(list->exit_signal == 0)
    {
    }
    assoc_array_delete_array(list->array,__delete_arwl);
    free(list);       
}

void arp_reply_waiting_list_add(struct arp_reply_waiting_list * list, struct sr_packet * packet,
                                uint32_t next_hop, const char * thru_interface)
{
    struct arwl_entry * entry;
    NEW_STRUCT(l_entry,arwl_list_entry);
    pthread_mutex_lock(&list->mutex);

    l_entry->packet = router_copy_packet(packet);
    l_entry->next_hop = next_hop;

    entry = (struct arwl_entry *)assoc_array_read(list->array,&next_hop);

    if(entry == NULL)
    {
        entry = (struct arwl_entry *)malloc(sizeof(struct arwl_entry));
        entry->sr = packet->sr;
        entry->next_hop = next_hop;
        entry->packet_list = fifo_create();
        assoc_array_insert(list->array,entry);
    }

    memcpy(entry->thru_interface, thru_interface, ETHER_ADDR_LEN);

    fifo_push(entry->packet_list,l_entry);
    
    pthread_mutex_unlock(&list->mutex);
}

void arp_reply_waiting_list_dispatch(struct arp_reply_waiting_list * list, uint32_t ip)
{
    struct arwl_entry * entry;
    struct arwl_list_entry * l_entry;
    pthread_mutex_lock(&list->mutex);
    entry = (struct arwl_entry *)assoc_array_delete(list->array,&ip);
    pthread_mutex_unlock(&list->mutex);
    
    if(entry)
    {
        while((l_entry = ((struct arwl_list_entry * )fifo_pop(entry->packet_list))))
        {
            ip_forward_packet(l_entry->packet,l_entry->next_hop,entry->thru_interface);
        }
        __delete_arwl(entry);
    }
}


void arp_request_handler_process_reply(struct sr_packet * packet)
{
    struct sr_arphdr * arp_hdr = ARP_HDR(packet);
    arp_cache_add(ROUTER(packet->sr)->a_cache,arp_hdr->ar_sip,arp_hdr->ar_sha);
    arp_reply_waiting_list_dispatch(ROUTER(packet->sr)->arwl, ARP_HDR(packet)->ar_sip);
}


void arp_request_handler_make_request(struct sr_packet * packet, uint32_t next_hop, const char * thru_interface)
{
    if(assoc_array_read(ROUTER(packet->sr)->arwl->array,&next_hop) == NULL)
    {
        arp_request(packet->sr,next_hop,thru_interface);
    }
    arp_reply_waiting_list_add(ROUTER(packet->sr)->arwl, packet,next_hop,thru_interface);
}
