#include <stdlib.h>
#include <string.h>
#include "router.h"

struct arp_reply_waiting_list * arp_reply_waiting_list_create()
{
    struct arp_reply_waiting_list * ret = (struct arp_reply_waiting_list *)malloc(sizeof(struct arp_reply_waiting_list));
    ret->array = assoc_array_create();
    pthread_mutex_init(&ret->mutex,NULL);
    return ret;
}

void __delete_arwl(void * data)
{
    struct arwl_entry * entry = (struct arwl_entry *) data;
    struct arwl_list_entry * l_entry;    
    while((l_entry = ((struct arwl_list_entry * )fifo_pop(entry->list))))
    {
        ip_forward_packet(l_entry->packet,l_entry->next_hop,l_entry->thru_interface);
        router_free_packet(l_entry->packet);
        free(l_entry);
    }
    free(entry->list);
    free(entry);
}

void arp_reply_waiting_list_destroy(struct arp_reply_waiting_list * list)
{
    pthread_mutex_destroy(&list->mutex);
    assoc_array_delete_array(list->array,__delete_arwl);
    free(list);       
}

void arp_reply_waiting_list_add(struct arp_reply_waiting_list * list, struct sr_packet * packet,
                                uint32_t next_hop, const char * thru_interface)
{
    struct arwl_entry * entry;
    struct arwl_list_entry * l_entry = (struct arwl_list_entry*)malloc(sizeof(struct arwl_list_entry));
    pthread_mutex_lock(&list->mutex);

    l_entry->packet = router_copy_packet(packet);
    l_entry->next_hop = next_hop;
    memcpy(l_entry->thru_interface, thru_interface, ETHER_ADDR_LEN);

    entry = (struct arwl_entry *)assoc_array_read(list->array,next_hop);

    if(entry == NULL)
    {
        entry = (struct arwl_entry *)malloc(sizeof(struct arwl_entry));
        entry->next_hop = next_hop;
        entry->list = fifo_create();
        assoc_array_insert(list->array,next_hop,entry);
    }

    fifo_push(entry->list,l_entry);
    
    pthread_mutex_unlock(&list->mutex);
}

void arp_reply_waiting_list_dispatch(struct arp_reply_waiting_list * list, uint32_t ip)
{
    struct arwl_entry * entry;
    struct arwl_list_entry * l_entry;
    pthread_mutex_lock(&list->mutex);

    entry = (struct arwl_entry *)assoc_array_delete(list->array,ip);
    if(entry)
    {
        while((l_entry = ((struct arwl_list_entry * )fifo_pop(entry->list))))
        {
            ip_forward_packet(l_entry->packet,l_entry->next_hop,l_entry->thru_interface);
            router_free_packet(l_entry->packet);
            free(l_entry);
        }
        free(entry->list);
        free(entry);
    }
    
    pthread_mutex_unlock(&list->mutex);
}
