#include "router.h"
#include <string.h>
#include <stdlib.h>
#include "sr_ifsys.h"
#include "nf2.h"
#include "lwtcp/lwip/sys.h"

#ifdef HAVE_WT
#include "server/server.h"
#endif

int router_thread_run(void * data, void * userdata)
{
    struct sr_thread * t = (struct sr_thread *)data;
    struct sr_instance * sr = (struct sr_instance *)userdata;

    if(t->run)
    {
        t->run(sr);
    }
    return 0;
}

int router_thread_end(void * data, void * userdata)
{
    struct sr_thread * t = (struct sr_thread *)data;
    struct sr_instance * sr = (struct sr_instance *)userdata;
    if(t->end)
    {
        t->end(sr);
    }
    return 0;
}

void router_thread(void * data)
{
    struct sr_instance * sr = (struct sr_instance *)data;

    while(1)
    {
        assoc_array_walk_array(ROUTER(sr)->threads,router_thread_run,sr);
        if(ROUTER(sr)->exit_signal == 0)
        {
            assoc_array_walk_array(ROUTER(sr)->threads,router_thread_end,sr);
            ROUTER(sr)->exit_signal = 1;
            return;
        }
        usleep(1000000);
    }
}


void router_add_thread(struct sr_instance * sr, void (*run)(struct sr_instance *),
                       void (*end)(struct sr_instance *))
{
    NEW_STRUCT(ret, sr_thread);
    ret->i = assoc_array_length(ROUTER(sr)->threads);
    ret->run = run;
    ret->end = end;

    assoc_array_insert(ROUTER(sr)->threads,ret);
}

struct sr_packet * router_construct_packet(struct sr_instance * sr,
                                           const uint8_t * packet, unsigned int len, const char* interface)
{
    struct sr_packet * ret = (struct sr_packet *)malloc(sizeof(struct sr_packet));

    ret->sr = sr;
    ret->packet = (uint8_t *) malloc(len*sizeof(uint8_t));
    memcpy(ret->packet,packet,len);
    ret->len = len;
    if(interface)
    {
        ret->interface = (char *)malloc(sizeof(char)*(strlen(interface) + 1));
        strcpy(ret->interface,interface);
    }
    else
    {
        ret->interface = NULL;
    }

    return ret;
}

void router_free_packet(struct sr_packet * packet)
{
    free(packet->packet);
    if(packet->interface)
    {
        free(packet->interface);
    }
    free(packet);
}

void router_handle_incoming_packet(struct sr_packet * packet)
{
    arp_cache_alert_packet_received(packet);
    switch(ntohs(ETH_HDR(packet)->ether_type))
    {
    case ETHERTYPE_ARP:
        arp_handle_incoming_packet(packet);
        break;
    case ETHERTYPE_IP:
        ip_handle_incoming_packet(packet);
        break;
    }    
}

void router_add_interface(struct sr_instance * sr, struct sr_vns_if * interface)
{
    struct sr_router * router = ROUTER(sr);
    struct ip_address ip;
    interface_list_add_interface(router->iflist,interface);

    ip.subnet = interface->ip & interface->mask;
    ip.mask   = interface->mask;
    forwarding_table_add(sr, &ip, 0, interface->name,1,0);
    if(interface_list_outbound(sr,interface->name))
    {
        forwarding_table_add(sr, &ip, 0, interface->name,1,1);
    }

    if(router->rid == 0)
    {
        router->rid = interface->ip;
    }

    strcpy(router->default_interface,interface->name);
}

void * router_thread_key(void * data)
{
    return &((struct sr_thread *)data)->i;
}

#ifdef HAVE_WT
static void __http_server_update(struct sr_instance * sr)
{
    if(router_get_http_port(sr)) server_update();
}
#endif

void router_create(struct sr_instance * sr, struct sr_options * opt)
{
    NEW_STRUCT(ret,sr_router);
    ret->threads = assoc_array_create(router_thread_key, assoc_array_key_comp_int);
    sr->router = ret;
    ret->exit_signal = 1;
    ret->hello = OSPF_DEFAULT_HELLOINT;
    ret->flood = OSPF_DEFAULT_LSUINT;

#ifdef _CPUMODE_
    strcpy(ret->device.device_name,DEFAULT_IFACE);
    if(check_iface(&ret->device) == 0)
    {
        if(openDescriptor(&ret->device) < 0)
        {
            fprintf(stderr, "Failed to open device %s", ret->device.device_name);
            exit(1);
        }
    }
    
    writeReg(&ret->device, CPCI_REG_CTRL, 0x00010100);
    usleep(2000);
#endif
    interface_list_create(sr);
    forwarding_table_create(sr);
    arp_cache_create(sr);
    arp_reply_waiting_list_create(sr);
    link_state_graph_create(sr);
    nat_create(sr);
    dhcp_create(sr);
    ret->rid = 0;
    ret->ospf_seq = 0;
    ret->opt = *opt;

    if(ret->opt.RCPPort != -1)
    {
        ret->rcp_server = RCPSeverCreate(sr, ret->opt.RCPPort);
    }
    else
    {
        ret->rcp_server = 0;
    }

#ifdef HAVE_WT
    router_add_thread(sr, __http_server_update, 0);
#endif
    sys_thread_new(router_thread,sr);

#ifdef HAVE_WT
    if(router_get_http_port(sr))
    {
        runserver(sr);
    }
#endif       
}

void router_destroy(struct sr_router * router)
{
    router->exit_signal = 0;
    while(router->exit_signal == 0)
    {
        usleep(1000);
    }
    assoc_array_delete_array(router->threads,assoc_array_delete_self);
    if(router->rcp_server)
    {
        RCPServerDestroy(router->rcp_server);
    }
    interface_list_destroy(router->iflist);
    forwarding_table_destroy(router->fwd_table);
    arp_cache_destroy(router->a_cache);
    arp_reply_waiting_list_destroy(router->arwl);
    link_state_graph_destroy(router->lsg);
    nat_destroy(router->nat);
    dhcp_destroy(router->dhcp);
    if(router->opt.inbound)
    {
        assoc_array_delete_array(router->opt.inbound, assoc_array_delete_self);
    }
    if(router->opt.outbound)
    {
        assoc_array_delete_array(router->opt.outbound, assoc_array_delete_self);
    }
    if(router->opt.ospf_disabled_interfaces)
    {
        assoc_array_delete_array(router->opt.ospf_disabled_interfaces, assoc_array_delete_self);
    }
    if(router->opt.dhcp)
    {
        assoc_array_delete_array(router->opt.dhcp, assoc_array_delete_self);
    }
#ifdef HAVE_WT
    if(router->opt.http_port)
    {
        stopserver();
        free(router->opt.http_port);
    }
#endif

#ifdef _CPUMODE_
    closeDescriptor(&router->device);
#endif
    free(router);
}


void router_load_static_routes(struct sr_instance * sr)
{
    struct sr_rt * rt_entry = sr->interface_subsystem->routing_table;
    struct ip_address ip;
        
    while(rt_entry)
    {
        ip.subnet = rt_entry->dest.s_addr;
        ip.mask   = rt_entry->mask.s_addr;
        forwarding_table_add(sr, &ip, rt_entry->gw.s_addr, rt_entry->interface,0,0);
        rt_entry = rt_entry->next;
    }
}

struct sr_packet * router_copy_packet(struct sr_packet * packet)
{
    return router_construct_packet(packet->sr, packet->packet, packet->len, packet->interface);
}


int router_cmp_MAC(void * k1, void * k2)
{
    uint8_t * i1 = (uint8_t *)k1;
    uint8_t * i2 = (uint8_t *)k2;
    int i = 0;
    for(;i < ETHER_ADDR_LEN; i++)
    {
        if(i1[i] < i2[i])
        {
            return ASSOC_ARRAY_KEY_LT;
        }
        else if(i1[i] > i2[i])
        {
            return ASSOC_ARRAY_KEY_GT;
        }
    }
    return ASSOC_ARRAY_KEY_EQ;
}

void sr_router_default_options(struct sr_options * opt)
{
    opt->arp_proxy = 0;
    opt->disable_ospf = 0;
    opt->aid = 0;

    opt->RCPPort = -1;
    opt->inbound = NULL;
    opt->outbound = NULL;
    opt->ospf_disabled_interfaces = NULL;
    opt->dhcp = NULL;

    opt->verbose = 0;
    opt->show_ip = 0;
    opt->show_arp = 0;
    opt->show_ospf = 0;
    opt->show_ospf_hello = 0;
    opt->show_ospf_lsu = 0;
    opt->show_icmp = 0;
    opt->show_tcp = 0;
    opt->show_udp = 0;
    opt->show_dhcp = 0;
    opt->http_port = NULL;
}

struct __nat_i
{
    struct sr_instance * sr;
    int nat;
};

static void __nat_a(struct sr_vns_if * iface, void * data)
{
    struct __nat_i * i = (struct __nat_i *)data;
    if(interface_list_nat_enabled(i->sr, iface->name))
    {
        i->nat = 1;
    }
}

int router_nat_enabled(struct sr_instance * sr)
{
    struct __nat_i i = {sr, 0};
    interface_list_loop_interfaces(sr, __nat_a, &i);
    return i.nat;
}

#ifdef HAVE_WT
const char * router_get_http_port(struct sr_instance * sr)
{
    return OPTIONS(sr)->http_port;
}
#endif

void router_notify(struct sr_instance * sr, int code)
{
    switch(code)
    {
    case ROUTER_UPDATE_FWD_TABLE:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_fwdtable();
#endif
        break;
    case ROUTER_UPDATE_FWD_TABLE_S:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_fwdtable_s();
#endif
        break;
    case ROUTER_UPDATE_ARP_TABLE:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_arptable(0);
#endif
        break;
    case ROUTER_UPDATE_ARP_TABLE_S:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_arptable_s();
#endif
        break;
    case ROUTER_UPDATE_ARP_TABLE_TTL:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_arptable(1);
#endif
        break;
    case ROUTER_UPDATE_IFACE_IP:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_ip();
#endif
        break;
    case ROUTER_UPDATE_ROUTER_TTL:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_router_ttl();
#endif
        break;
    case ROUTER_UPDATE_OSPF:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_ospf();
#endif
        break;
    case ROUTER_UPDATE_OSPF_TTL:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_ospf_ttl();
#endif
        break;
    case ROUTER_UPDATE_NAT:
#ifdef HAVE_WT
        if(router_get_http_port(sr)) server_update_nat();
#endif
        break;        
    }
}

uint32_t router_get_aid(struct sr_instance * sr)
{
    uint32_t aid;
    interface_list_get_params(sr, ROUTER(sr)->default_interface, &aid, 0, 0, 0);
    return aid;
}

struct __router_set_aid_i
{
    struct sr_instance * sr;
    uint32_t aid;
};

void __router_set_aid_a(struct sr_vns_if * iface, void * data)
{
    struct __router_set_aid_i * ai = (struct __router_set_aid_i *)data;
    interface_list_set_aid(ai->sr, iface->name, ai->aid);
}

void router_set_aid(struct sr_instance * sr, uint32_t aid)
{
    struct __router_set_aid_i ai = {sr,aid};
    interface_list_loop_interfaces(sr, __router_set_aid_a, &ai);
    router_notify(sr, ROUTER_UPDATE_IFACE_IP);
}

void router_set_rid(struct sr_instance * sr, uint32_t rid)
{
    ROUTER(sr)->rid = rid;
}

void router_set_ospf_info(struct sr_instance * sr, int * hello, int * flood)
{
    if(hello) ROUTER(sr)->hello = *hello;
    if(flood) ROUTER(sr)->flood = *flood;
}

void router_get_ospf_info(struct sr_instance * sr, int * hello, int * flood)
{
    if(hello) *hello = ROUTER(sr)->hello;
    if(flood) *flood = ROUTER(sr)->flood;
}
