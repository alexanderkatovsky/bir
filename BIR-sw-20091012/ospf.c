#include "router.h"

void ospf_construct_ospf_header(uint8_t * packet, uint8_t type, uint16_t len, uint32_t rid, uint32_t aid)
{
    struct ospfv2_hdr * ospf_hdr = B_OSPF_HDR(packet);
    uint16_t ospf_len = len - (sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
    ospf_hdr->version = 2;
    ospf_hdr->type = type;
    ospf_hdr->len = htons(ospf_len);
    ospf_hdr->rid = rid;
    ospf_hdr->aid = htonl(aid);
    ospf_hdr->autype = 0;
    ospf_hdr->audata = 0;    
    ospf_hdr->csum = checksum_ospfheader((const uint8_t *)ospf_hdr,ospf_len);
}

void ospf_construct_hello_header(uint8_t * packet, uint32_t mask, uint16_t helloint)
{
    struct ospfv2_hello_hdr * hdr = B_HELLO_HDR(packet);
    hdr->nmask = mask;
    hdr->helloint = htons(helloint);
    hdr->padding = 0;
}

void ospf_construct_lsu_header(uint8_t * packet, uint16_t seq, uint32_t num_adv)
{
    struct ospfv2_lsu_hdr * hdr = B_LSU_HDR(packet);
    hdr->seq = htons(seq);
    hdr->unused = 0;
    hdr->ttl = OSPF_MAX_LSU_TTL;
    hdr->num_adv = htonl(num_adv);
}

void ospf_send_hello(struct sr_instance * sr, struct interface_list_entry * ifentry)
{
    int len = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) +
        sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr);

    uint8_t * packet = (uint8_t *)malloc(len);
    ospf_construct_hello_header(packet,ifentry->vns_if->mask,OSPF_DEFAULT_HELLOINT);
    ospf_construct_ospf_header(packet,OSPF_TYPE_HELLO,len,ROUTER(sr)->rid,ifentry->aid);
    ip_construct_ip_header(packet,len,0,OSPF_MAX_LSU_TTL,IP_P_OSPF,ifentry->vns_if->ip,htonl(OSPF_AllSPFRouters));
    ip_construct_eth_header(packet,0,ifentry->vns_if->addr,ETHERTYPE_IP);

    if(sr_integ_low_level_output(sr,packet,len,ifentry->vns_if->name) == -1)
    {
        printf("\nfailed to send packet\n");
    }
    free(packet);
}

void __ospf_forward_incoming_lsu_a(struct sr_vns_if * vns_if, struct neighbour * n, void * userdata)
{
    struct sr_packet * packet = (struct sr_packet *)userdata;
    struct in_addr src = {vns_if->ip};
    struct in_addr dst = {n->ip};
    IP_HDR(packet)->ip_src = src;
    IP_HDR(packet)->ip_dst = dst;
    ip_forward_packet(packet,n->ip,vns_if->name);
}

void ospf_forward_incoming_lsu(struct sr_packet * packet)
{
    interface_list_loop_through_neighbours(INTERFACE_LIST(packet->sr), __ospf_forward_incoming_lsu_a, packet);
}

void ospf_handle_incoming_lsu(struct sr_packet * packet)
{
    struct ospfv2_hdr * ospf_hdr = OSPF_HDR(packet);
    struct ospfv2_lsu_hdr * lsu_hdr = LSU_HDR(packet);
    if((ospf_hdr->rid != 0) && (ospf_hdr->rid != ROUTER(packet->sr)->rid))
    {
        if(link_state_graph_update_links(packet->sr, ospf_hdr->rid,ntohs(lsu_hdr->seq),
                                         ntohl(lsu_hdr->num_adv),LSU_START(packet)))
        {
            ospf_forward_incoming_lsu(packet);
        }
    }
}

void ospf_handle_incoming_hello(struct sr_packet * packet)
{
    struct ip * ip_hdr = IP_HDR(packet);
    struct ospfv2_hdr * ospf_hdr = OSPF_HDR(packet);
    struct ospfv2_hello_hdr * hello_hdr = HELLO_HDR(packet);

    interface_list_process_incoming_hello(packet->sr,
                                          packet->interface, ip_hdr->ip_src.s_addr,
                                          ospf_hdr->rid,
                                          ntohl(ospf_hdr->aid),
                                          hello_hdr->nmask,
                                          ntohs(hello_hdr->helloint));
}

void ospf_handle_incoming_packet(struct sr_packet * packet)
{
    struct ospfv2_hdr * ospf_hdr = OSPF_HDR(packet);
    unsigned short csum = checksum_ospfheader((const uint8_t *)ospf_hdr,
                                              packet->len - (sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)));

    if((ospf_hdr->version == 2) && (ospf_hdr->csum == csum))
    {
        switch(ospf_hdr->type)
        {
        case OSPF_TYPE_HELLO:
            ospf_handle_incoming_hello(packet);
            break;
        case OSPF_TYPE_LSU:
            ospf_handle_incoming_lsu(packet);
            break;
        }
    }
}
