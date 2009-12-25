#include "router.h"


int dhcp_packet(struct sr_packet * packet)
{
    return
        IP_HDR(packet)->ip_p == IP_P_UDP &&
        ntohs(UDP_HDR(packet)->dst_port) == 67;        
}

int dhcp_read_option(struct sr_packet * packet, uint8_t ** data, int * len, uint8_t message_type)
{
    int opt_len = ntohs(UDP_HDR(packet)->len) - sizeof(struct udp_header) - sizeof(struct dhcp_header);
    int t_len = 0;
    uint8_t * start = ((uint8_t *)DHCP_HDR(packet)) + sizeof(struct dhcp_header);
    int found = 0;

    while(!found && (t_len < opt_len))
    {
        if(message_type == start[0])
        {
            *len = start[1];
            *data = start + 2;
            found = 1;
        }
        else
        {
            t_len += start[1];
            start += 2 + start[1];
        }
    }

    if(!found)
    {
        *data = NULL;
        *len = 0;
    }
    return found;
}

int dhcp_message_type(struct sr_packet * packet)
{
    int len;
    uint8_t * data;
    int ret = -1;

    dhcp_read_option(packet,&data, &len, DHCP_OPTION_MESSAGE_TYPE);
    if(data)
    {
        ret = *data;
    }
    return ret;
}

void dhcp_add_option(uint8_t ** data, int * len, uint8_t opt_num, uint8_t * opt_data, int opt_len)
{
    uint8_t * data2 = malloc(*len + opt_len + 2);
    memcpy(data2, *data, *len);
    free(*data);
    *data = data2;
    (*data)[*len] = opt_num;
    (*data)[*len + 1] = opt_len;
    memcpy(*data + (*len + 2), opt_data, opt_len);
    *len += opt_len + 2;
    B_UDP_HDR(*data)->len = htons(*len - sizeof(struct ip) - sizeof(struct sr_ethernet_hdr));
    B_IP_HDR(*data)->ip_len = htons(*len - sizeof(struct sr_ethernet_hdr));
    B_IP_HDR(*data)->ip_sum = checksum_ipheader(B_IP_HDR(*data));
}

void dhcp_construct_packet(uint8_t ** data, int * len, struct sr_packet * packet, uint32_t yip, uint8_t opt_type,
                           uint32_t server_ip, uint32_t mask, uint8_t * c_mac)
{
    uint32_t src_ip;
    uint32_t lease_time = 86400;
    *len = sizeof(struct dhcp_header) + sizeof(struct udp_header) + sizeof(struct ip)
        + sizeof(struct sr_ethernet_hdr);
    *data = malloc(*len);
    memset(*data,0,*len);
    ip_construct_eth_header(*data,c_mac,NULL,ETHERTYPE_IP);
    interface_list_get_MAC_and_IP_from_name(INTERFACE_LIST(packet->sr),packet->interface,
                                            B_ETH_HDR(*data)->ether_shost,&src_ip);
    ip_construct_ip_header(*data,*len - sizeof(struct sr_ethernet_hdr),
                           ntohs(B_IP_HDR(packet)->ip_id),63,IP_P_UDP,src_ip,0xffffffff);
    ip_construct_udp_header(*data,*len - sizeof(struct sr_ethernet_hdr) - sizeof(struct ip),67,68);


    B_DHCP_HDR(*data)->op = 2;
    B_DHCP_HDR(*data)->htype = 1;
    B_DHCP_HDR(*data)->hlen = 6;
    B_DHCP_HDR(*data)->hops = 0;
    B_DHCP_HDR(*data)->op = 2;
    B_DHCP_HDR(*data)->xid = DHCP_HDR(packet)->xid;
    B_DHCP_HDR(*data)->secs = 0;
    B_DHCP_HDR(*data)->flags = 0;
    B_DHCP_HDR(*data)->c_ip = 0;
    B_DHCP_HDR(*data)->y_ip = yip;
    B_DHCP_HDR(*data)->s_ip = src_ip;
    B_DHCP_HDR(*data)->g_ip = 0;
    memcpy(B_DHCP_HDR(*data)->c_mac, DHCP_HDR(packet)->c_mac, 16);
    B_DHCP_HDR(*data)->magic = DHCP_HDR(packet)->magic;

    dhcp_add_option(data, len, 53, &opt_type, 1);
    dhcp_add_option(data, len, 1, (uint8_t *)&mask, 4);
    dhcp_add_option(data, len, 3, (uint8_t *)&server_ip, 4);
    dhcp_add_option(data, len, 51,(uint8_t *)&lease_time, 4);
    dhcp_add_option(data, len, 54,(uint8_t *)&ROUTER(packet->sr)->rid, 4);
}

void dhcp_reject(struct sr_packet * packet)
{
}

uint32_t dhcp_alloc_ip(struct sr_instance * sr, char * interface)
{
    struct dhcp_s * d = assoc_array_read(OPTIONS(sr)->dhcp, interface);
    struct bi_assoc_array * array = DHCP(sr)->array;

    uint32_t ret = 0;
    int i = d->from;
    struct dhcp_table_key dk;
    strcpy(dk.interface, interface);
    for(; i <= d->to; i++)
    {
        dk.ip = i;
        if(bi_assoc_array_read_1(array,&dk) == NULL)
        {
            ret = dk.ip;
            break;
        }
    }
    return ret;
}

void dhcp_handle(struct sr_packet * packet, uint8_t type)
{
    struct dhcp_table_entry * entry = bi_assoc_array_read_2(DHCP(packet->sr)->array,
                                                            DHCP_HDR(packet)->c_mac);

    struct sr_vns_if * vnsif = interface_list_get_interface_by_name(INTERFACE_LIST(packet->sr), packet->interface);
    uint8_t * data;
    int len;
    uint8_t * p_req_ip;
    uint32_t req_ip;
    int req_len;
    struct dhcp_s * d = assoc_array_read(OPTIONS(packet->sr)->dhcp, packet->interface);
    if(entry == NULL)
    {
        entry = (struct dhcp_table_entry *)malloc(sizeof(struct dhcp_table_entry));
        memcpy(entry->MAC,DHCP_HDR(packet)->c_mac,ETHER_ADDR_LEN);
        strcpy(entry->key.interface,packet->interface);
        entry->key.ip = dhcp_alloc_ip(packet->sr,packet->interface);
        if(entry->key.ip == 0)
        {
            dhcp_reject(packet);
            return;
        }
        bi_assoc_array_insert(DHCP(packet->sr)->array, entry);
    }

    /* try to allocate request in ACK */
    if(type == DHCP_ACK && dhcp_read_option(packet,&p_req_ip, &req_len, 50) && req_len == 4)
    {
        req_ip = *((uint32_t *)p_req_ip);
        if(req_ip >= d->from && req_ip <= d->to)
        {
            entry->key.ip = req_ip;
        }
    }

    dhcp_construct_packet(&data, &len, packet,entry->key.ip,type,vnsif->ip,vnsif->mask,
                          DHCP_HDR(packet)->c_mac);

    if(sr_integ_low_level_output(packet->sr,data,len,packet->interface) == -1)
    {
        printf("\nfailed to send packet\n");
    }
    free(data);
}

void dhcp_handle_incoming(struct sr_packet * packet)
{
    struct dhcp_s * d = assoc_array_read(OPTIONS(packet->sr)->dhcp, packet->interface);
    mutex_lock(DHCP(packet->sr)->mutex);

    if(d) /* if this interface is DHCP enabled */
    {
        switch(dhcp_message_type(packet))
        {
        case DHCP_DISCOVER:
            dhcp_handle(packet, DHCP_OFFER);
            break;
        case DHCP_REQUEST:
            dhcp_handle(packet, DHCP_ACK);
            break;
        }
    }
    mutex_unlock(DHCP(packet->sr)->mutex);
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
        cmp = assoc_array_key_comp_int(&key1->ip, &key2->ip);
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
