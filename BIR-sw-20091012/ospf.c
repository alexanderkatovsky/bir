#include "router.h"

void ospf_construct_ospf_header(uint8_t * packet, uint8_t type, uint16_t len, uint32_t rid, uint32_t aid)
{
    struct ospfv2_hdr * ospf_hdr = B_OSPF_HDR(packet);
    uint16_t ospf_len = len - (sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
    ospf_hdr->version = 2;
    ospf_hdr->type = type;
    ospf_hdr->len = htons(ospf_len);
    ospf_hdr->rid = htonl(rid);
    ospf_hdr->aid = htonl(aid);
    ospf_hdr->csum = checksum_ospfheader((const uint8_t *)ospf_hdr,ospf_len);
    ospf_hdr->autype = 0;
    ospf_hdr->audata = 0;
}

void ospf_construct_hello_header(uint8_t * packet, uint32_t mask, uint16_t helloint)
{
    struct ospfv2_hello_hdr * hdr = B_HELLO_HDR(packet);
    hdr->nmask = mask;
    hdr->helloint = htons(helloint);
    hdr->padding = 0;
}

void ospf_send_hello(struct sr_instance * sr, struct interface_list_entry * ifentry)
{
    int len = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) +
        sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr);

    uint8_t * packet = (uint8_t *)malloc(len);
    ip_construct_eth_header(packet,0,ifentry->vns_if->addr,ETHERTYPE_IP);
    ip_construct_ip_header(packet,len,0,OSPF_MAX_LSU_TTL,IP_P_OSPF,ifentry->vns_if->ip,htonl(OSPF_AllSPFRouters));
    ospf_construct_ospf_header(packet,OSPF_TYPE_HELLO,len,htonl(ROUTER(sr)->rid),ifentry->aid);
    ospf_construct_hello_header(packet,ifentry->vns_if->mask,OSPF_DEFAULT_HELLOINT);
    if(sr_integ_low_level_output(sr,packet,len,ifentry->vns_if->name) == -1)
    {
        printf("\nfailed to send packet\n");
    }
    free(packet);
}

void ospf_handle_incoming_lsu(struct sr_packet * packet)
{
    struct ospfv2_hdr * ospf_hdr = OSPF_HDR(packet);
    struct ospfv2_lsu_hdr * lsu_hdr = LSU_HDR(packet);
    if(ospf_hdr->rid != 0)
    {
        link_state_graph_update_links(packet->sr, ospf_hdr->rid,ntohs(lsu_hdr->seq),
                                      ntohl(lsu_hdr->num_adv),LSU_START(packet));
    }
}

void ospf_handle_incoming_hello(struct sr_packet * packet)
{
    struct ip * ip_hdr = IP_HDR(packet);
    struct ospfv2_hdr * ospf_hdr = OSPF_HDR(packet);
    struct ospfv2_hello_hdr * hello_hdr = HELLO_HDR(packet);

    interface_list_process_incoming_hello(packet, ROUTER(packet->sr)->iflist,
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
                                              packet->len - ((sizeof(struct sr_ethernet_hdr) + sizeof(struct ip))));

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
