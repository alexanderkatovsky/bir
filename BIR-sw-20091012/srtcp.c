#include "router.h"
#include "nat.h"

void sr_transport_input(uint8_t* packet /* borrowed */);

uint16_t tcp_checksum(uint8_t * tcphdr, uint32_t src_ip, uint32_t dst_ip, uint16_t len)
{
    struct tcp_pseudo_hdr ps = {src_ip,dst_ip,0,IP_P_TCP,htons(len)};
    uint8_t * data = malloc(sizeof(struct tcp_pseudo_hdr) + len);
    struct tcpheader * tcp_hdr;
    uint16_t ret;
    memcpy(data, &ps, sizeof(struct tcp_pseudo_hdr));
    memcpy(data + sizeof(struct tcp_pseudo_hdr), tcphdr, len);
    tcp_hdr = (struct tcpheader *)(data + sizeof(struct tcp_pseudo_hdr));
    tcp_hdr->tcph_chksum = 0;
    ret = cksum((const unsigned short *)data, len + sizeof(struct tcp_pseudo_hdr));
    free(data);
    return ret;
}

int tcp_check_packet(struct sr_packet * packet)
{
    uint16_t cksum;
    if(IP_HDR(packet)->ip_p == IP_P_UDP) return 1;

    cksum = tcp_checksum((uint8_t *)TCP_HDR(packet), IP_HDR(packet)->ip_src.s_addr,
                         IP_HDR(packet)->ip_dst.s_addr,
                         packet->len - sizeof(struct sr_ethernet_hdr) - sizeof(struct ip));
    if(cksum != TCP_HDR(packet)->tcph_chksum)
    {
        printf("\nTCP Checksums differ: %x,%x\n", TCP_HDR(packet)->tcph_chksum, cksum);
        return 0;
    }
    return 1;
}

struct sr_packet * tcp_construct_packet(struct sr_packet * packet, uint32_t src_ip,
                                        int src_port, uint32_t dst_ip, int dst_port, char * thru_interface)
{
    uint8_t * packet_data = malloc(packet->len);
    struct sr_packet * ret;
    memcpy(packet_data, packet->packet,packet->len);

    if(IP_HDR(packet)->ip_p == IP_P_TCP)
    {
        B_TCP_HDR(packet_data)->tcph_srcport = htons(src_port);
        B_TCP_HDR(packet_data)->tcph_destport = htons(dst_port);
        B_TCP_HDR(packet_data)->tcph_chksum = tcp_checksum((uint8_t *)B_TCP_HDR(packet_data), src_ip, dst_ip,
                                                           packet->len - sizeof(struct sr_ethernet_hdr) - sizeof(struct ip));
    }
    else if(IP_HDR(packet)->ip_p == IP_P_UDP)
    {
        B_UDP_HDR(packet_data)->src_port = htons(src_port);
        B_UDP_HDR(packet_data)->dst_port = htons(dst_port);
        B_UDP_HDR(packet_data)->cksum = 0;
    }
    B_IP_HDR(packet_data)->ip_src.s_addr = src_ip;
    B_IP_HDR(packet_data)->ip_dst.s_addr = dst_ip;
    B_IP_HDR(packet_data)->ip_sum = checksum_ipheader(B_IP_HDR(packet_data));
    
    ret = router_construct_packet(packet->sr, packet_data, packet->len, thru_interface);    
    free(packet_data);
    return ret;
}

void tcp_handle_incoming_not_for_us(struct sr_packet * packet)
{
    uint32_t next_hop;
    char thru_interface[SR_NAMELEN];
    int istcp = (IP_HDR(packet)->ip_p == IP_P_TCP);
    uint16_t src_port = ntohs(istcp ? TCP_HDR(packet)->tcph_srcport : UDP_HDR(packet)->src_port);
    uint16_t dst_port = ntohs(istcp ? TCP_HDR(packet)->tcph_destport : UDP_HDR(packet)->dst_port);
    uint32_t src_ip = IP_HDR(packet)->ip_src.s_addr;
    uint32_t dst_ip = IP_HDR(packet)->ip_dst.s_addr;
    struct sr_packet * packet2;

    if(tcp_check_packet(packet))
    {
        if(interface_list_inbound(packet->sr, packet->interface))
        {
            if(forwarding_table_lookup_next_hop(ROUTER(packet->sr)->fwd_table, dst_ip,
                                                &next_hop, thru_interface, 1))
            {
                if(interface_list_outbound(packet->sr, thru_interface))
                {
                    /* allocate out_port, lookup src_ip from thru_interface, and add to NAT table  */
                    if(nat_out(packet->sr,&src_ip,&src_port,dst_ip,dst_port,thru_interface))
                    {
                        packet2 = tcp_construct_packet(packet,src_ip,src_port,dst_ip,dst_port,thru_interface);
                        ip_forward(packet2);
                        router_free_packet(packet2);
                    }
                }
                else if(strcmp(thru_interface, packet->interface) == 0 ||
                        !interface_list_inbound(packet->sr, thru_interface))
                {
                    ip_forward(packet);
                }
            }
            else
            {
                icmp_send_host_unreachable(packet);
            }
        }
        else
        {
            ip_forward(packet);
        }
    }
}

/* from outbound  */
void tcp_handle_incoming_for_us(struct sr_packet * packet)
{
    uint32_t next_hop;
    char thru_interface[SR_NAMELEN];
    int istcp = (IP_HDR(packet)->ip_p == IP_P_TCP);
    uint16_t src_port = ntohs(istcp ? TCP_HDR(packet)->tcph_srcport : UDP_HDR(packet)->src_port);
    uint16_t dst_port = ntohs(istcp ? TCP_HDR(packet)->tcph_destport : UDP_HDR(packet)->dst_port);    
    uint32_t src_ip = IP_HDR(packet)->ip_src.s_addr;
    uint32_t dst_ip = IP_HDR(packet)->ip_dst.s_addr;
    struct sr_packet * packet2;

    if(tcp_check_packet(packet))
    {
        if(!interface_list_outbound(packet->sr, packet->interface))
        {
            sr_transport_input(packet->packet);
        }
        else if(nat_in(packet->sr,src_ip,src_port,&dst_ip,&dst_port))
        {
            if(forwarding_table_lookup_next_hop(ROUTER(packet->sr)->fwd_table, dst_ip,
                                                &next_hop, thru_interface, 0))
            {
                packet2 = tcp_construct_packet(packet,src_ip,src_port,dst_ip,dst_port, thru_interface);
                ip_send(packet2,next_hop,thru_interface);
                router_free_packet(packet2);            
            }
        }
        else
        {
            sr_transport_input(packet->packet);
        }
    }
}
