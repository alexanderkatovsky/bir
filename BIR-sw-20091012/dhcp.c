#include "router.h"


int dhcp_packet(struct sr_packet * packet)
{
    return
        IP_HDR(packet)->ip_p == IP_P_UDP &&
        UDP_HDR(packet)->dst_port == 67;        
}

void dhcp_handle_incoming(struct sr_packet * packet)
{
    struct dhcp_s * d = assoc_array_read(OPTIONS(packet->sr)->dhcp, packet->interface);

    if(d)
    {
        
    }
}

void * dhcp_s_get_key(void * data)
{
    return ((struct dhcp_s *)data)->name;
}

int dhcp_key_cmp(void * k1, void * k2)
{
    struct dhcp_table_key * key1 = (struct dhcp_table_key *)k1;
    struct dhcp_table_key * key2 = (struct dhcp_table_key *)k2;

    int cmp = assoc_array_key_comp_str(key1->interface,key2->interface);
    if(cmp == ASSOC_ARRAY_KEY_EQ)
    {
        cmp = ip_address_cmp(&key1->ip, &key2->ip);
    }
    return cmp;
}

void * dhcp_get_key(void * data)
{
    return &((struct dhcp_table_entry *)data)->key;
}

void * dhcp_get_MAC(void * data)
{
    return ((struct dhcp_table_entry *)data)->MAC;
}

void dhcp_create(struct sr_instance * sr)
{
    NEW_STRUCT(ret,dhcp_table);
    ROUTER(sr)->dhcp = ret;
    ret->array = bi_assoc_array_create(dhcp_get_key,dhcp_key_cmp,
                                       dhcp_get_MAC,router_cmp_MAC);
    
    ret->mutex = mutex_create();
}

void dhcp_destroy(struct dhcp_table * dhcp)
{
    bi_assoc_array_delete_array(dhcp->array, assoc_array_delete_self);
    mutex_destroy(dhcp->mutex);
    free(dhcp);
}
