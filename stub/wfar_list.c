#include "grizly.h"

/*
 * WFAR = Wait For Arp Reply
 * this saves a packet that is waiting for an ARP reply to be sent
 * (prev) <- wfar_list <-> n <-> n <-> ... <-> n -> (next)
 *
 * */

struct wfar_node
{
    struct sr_instance * inst;
    uint8_t * packet;
    unsigned int len;
    uint32_t next_hop;
    char * interface;

    struct wfar_node * next;
    struct wfar_node * prev;
};


struct wfar_node * wfar_list;
int wfar_list_semaphore;

#define THREAD_SAFE(code)                       \
    while(wfar_list_semaphore == 1){}           \
    wfar_list_semaphore = 1;                    \
    code                                        \
    wfar_list_semaphore = 0;                    


void init_wfar_list()
{
    wfar_list = NULL;
    wfar_list_semaphore = 0;
}

void clear_wfar_list()
{
    struct wfar_node * node = wfar_list;
    struct wfar_node * next;

    THREAD_SAFE(
        
        while(node != NULL)
        {
            next = node->next;
            free(node);
            node = next;
        }
        
        if(node != NULL)
        {
            free(node);
        }

        wfar_list = NULL;
        )
}


void add_to_wfar_list(struct sr_instance * inst,uint8_t * packet, unsigned int len, uint32_t next_hop, char* interface)
{
    struct wfar_node * node = (struct wfar_node *)malloc(sizeof(struct wfar_node));
    node->inst = inst;
    node->packet = (uint8_t *)malloc(len);
    memcpy(node->packet,packet,len);
    node->len = len;
    node->next_hop = next_hop;
    node->interface = interface;
    node->next = NULL;
    node->prev = NULL;
    
    THREAD_SAFE(
    
        if(wfar_list == NULL)
        {
            wfar_list = node;
        }
        else
        {
            node->next = wfar_list;
            wfar_list->prev = node;
            wfar_list = node;
        }
        )
}

struct wfar_node * wfar_remove_node(struct wfar_node * node)
{
    struct wfar_node * next;
    free(node->packet);
    if(node->next)
    {
        node->next->prev = node->prev;
    }
    if(node->prev)
    {
        node->prev->next = node->next;
    }
    next = node->next;
    if(node == wfar_list)
    {
        wfar_list = node->next;
    }

    free(node);
    return next;
}


/* send every packet whose next hop is ip */
void process_wfar_list_on_arp_reply(uint32_t ip)
{
    struct wfar_node * node = wfar_list;


    THREAD_SAFE(
        while(node->next)
        {
            node = node->next;
        }
        
        while(node)
        {
            if(node->next_hop == ip)
            {
                forward_packet(node->inst,node->packet,node->len,node->next_hop,node->interface);
                node = wfar_remove_node(node);
            }
            else
            {
                node = node->prev;
            }
        }
        )
}

