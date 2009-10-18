#include "grizly.h"

struct arp_node
{
    uint32_t ip;
    unsigned char ha[ETHER_ADDR_LEN];

    struct arp_node * next;
};

struct arp_node * arp_cache;
int arp_cache_semaphore;

#define THREAD_SAFE(code)                       \
    while(arp_cache_semaphore == 1){}           \
    arp_cache_semaphore = 1;                    \
    code                                        \
    arp_cache_semaphore = 0;                    


void init_arp_cache()
{
    arp_cache = NULL;
    arp_cache_semaphore = 0;
    /* create thread that clears the cache every 15 seconds  */
}

void clear_arp_cache()
{
    struct arp_node * node = arp_cache;
    struct arp_node * next;

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

        arp_cache = NULL;
        )
}


void add_to_arp_cache(uint32_t ip, unsigned char * ha)
{
    struct arp_node * node = (struct arp_node *)malloc(sizeof(struct arp_node));
    node->ip = ip;
    memcpy(node->ha,ha,ETHER_ADDR_LEN);
    node->next = NULL;
    
    THREAD_SAFE(
    
        if(arp_cache == NULL)
        {
            arp_cache = node;
        }
        else
        {
            node->next = arp_cache;
            arp_cache = node;
        }
        )
}

int get_MAC_from_arp_cache(uint32_t ip, unsigned char * ha)
{
    int ret = 0;
    struct arp_node * node = arp_cache;

    THREAD_SAFE(
        
        while(!ret && node)
        {
            if(node->ip == ip)
            {
                ret = 1;
                memcpy(ha,node->ha,ETHER_ADDR_LEN);
            }
            node = node->next;
        }
        )

    return ret;
}
