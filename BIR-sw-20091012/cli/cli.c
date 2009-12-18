/* Filename: cli.c */

#include <signal.h>
#include <stdio.h>               /* snprintf()                        */
#include <stdlib.h>              /* malloc()                          */
#include <string.h>              /* strncpy()                         */
#include <sys/time.h>            /* struct timeval                    */
#include <unistd.h>              /* sleep()                           */
#include "cli.h"
#include "cli_network.h"         /* make_thread()                     */
#include "helper.h"
#include "socket_helper.h"       /* writenstr()                       */
#include "../sr_base_internal.h" /* struct sr_instance                */
#include "../router.h"
#include "../reg_defines.h"

/* temporary */
#include "cli_stubs.h"

/** whether to shutdown the server or not */
static int router_shutdown;

/** socket file descriptor where responses should be sent */
static int fd;

/** whether the fd is was terminated */
static int fd_alive;

/** whether the client is in verbose mode */
static int* pverbose;

/** whether to skip next prompt call */
static int skip_next_prompt;

#ifdef _STANDALONE_CLI_
/**
 * Initialize sr for the standalone binary which just runs the CLI.
 */
struct sr_instance* my_get_sr() {
    static struct sr_instance* sr = NULL;
    if( ! sr ) {
        sr = malloc( sizeof(*sr) );
        true_or_die( sr!=NULL, "malloc falied in my_get_sr" );

        fprintf( stderr, "not yet implemented: my_get_sr does not create a value for sr->interface_subsystem\n" );
        sr->interface_subsystem = NULL;

        sr->topo_id = 0;
        strncpy( sr->vhost, "cli", SR_NAMELEN );
        strncpy( sr->user, "cli mode (no client)", SR_NAMELEN );
        if( gethostname(sr->lhost,  SR_NAMELEN) == -1 )
            strncpy( sr->lhost, "cli mode (unknown localhost)", SR_NAMELEN );

        /* NOTE: you probably want to set some dummy values for the rtable and
           interface list of your interface_subsystem here (preferably read them
           from a file) */
    }

    return sr;
}
#   define SR my_get_sr()
#else
#   include "../sr_integration.h" /* sr_get() */
#   define SR get_sr()
#endif


/**
 * Wrapper for writenstr.  Tries to send the specified string with the
 * file-scope fd.  If it fails, fd_alive is set to 0.  Does nothing if
 * fd_alive is already 0.
 */
static void cli_send_str( const char* str ) {
    if( fd_alive )
        if( 0 != writenstr( fd, str ) )
            fd_alive = 0;
}


static int cli_printf(const char * format, ...)
{
    int n = -1, size = 100;
    char *p, *np;
    va_list ap;
    int end = 0;

    p = malloc (size);

    while (end == 0)
    {
        va_start(ap, format);
        n = vsnprintf (p, size, format, ap);
        va_end(ap);

        if (n > -1 && n < size)
        {
            end = 1;
        }
        else
        {
            if (n > -1)
            {
                size = n+1;
            }
            else
            {
                size *= 2;
            }
            if ((np = realloc (p, size)) == NULL)
            {
                free(p);
                p = NULL;
                n = -1;
                end = 1;
            }
            else
            {
                p = np;
            }
        }
    }

    if(p)
    {
        cli_send_str(p);
    }

    return n;
}

#ifdef _VNS_MODE_
/**
 * Wrapper for writenstr.  Tries to send the specified string followed by a
 * newline with the file-scope fd.  If it fails, fd_alive is set to 0.  Does
 * nothing if fd_alive is already 0.
 */
static void cli_send_strln( const char* str ) {
    if( fd_alive )
        if( 0 != writenstrs( fd, 2, str, "\n" ) )
            fd_alive = 0;
}
#endif

/**
 * Wrapper for writenstrs.  Tries to send the specified string(s) with the
 * file-scope fd.  If it fails, fd_alive is set to 0.  Does nothing if
 * fd_alive is already 0.
 */
static void cli_send_strs( int num_args, ... ) {
    const char* str;
    int ret;
    va_list args;

    if( !fd_alive ) return;
    va_start( args, num_args );

    ret = 0;
    while( ret==0 && num_args-- > 0 ) {
        str = va_arg(args, const char*);
        ret = writenstr( fd, str );
    }

    va_end( args );
    if( ret != 0 )
        fd_alive = 0;
}

void cli_init() {
    router_shutdown = 0;
    skip_next_prompt = 0;
}

int cli_is_time_to_shutdown() {
    return router_shutdown;
}

int cli_focus_is_alive() {
    return fd_alive;
}

void cli_focus_set( const int sfd, int* verbose ) {
    fd_alive = 1;
    fd = sfd;
    pverbose = verbose;
}

void cli_send_help( cli_help_t help_type ) {
    if( fd_alive )
        if( !cli_send_help_to( fd, help_type ) )
            fd_alive = 0;
}

void cli_send_parse_error( int num_args, ... ) {
    const char* str;
    int ret;
    va_list args;

    if( fd_alive ) {
        va_start( args, num_args );

        ret = 0;
        while( ret==0 && num_args-- > 0 ) {
            str = va_arg(args, const char*);
            ret = writenstr( fd, str );
        }

        va_end( args );
        if( ret != 0 )
            fd_alive = 0;
    }
}

void cli_send_welcome() {
    cli_send_str( "You are now logged into the router CLI.\n" );
}

void cli_send_prompt() {
    if( !skip_next_prompt )
        cli_send_str( PROMPT );

    skip_next_prompt = 0;
}

void cli_print_rid()
{
    cli_printf("\nthis router id: ");print_ip(ROUTER(get_sr())->rid,cli_printf);cli_printf("\n\n");
}

void cli_show_all() {
#ifdef _CPUMODE_
    cli_show_hw();
    cli_send_str( "\n" );
#endif
    cli_show_ip();
#ifndef _CPUMODE_
#ifndef _MANUAL_MODE_
    cli_send_str( "\n" );
    cli_show_vns();
#endif
#endif
}

#ifndef _CPUMODE_
void cli_send_no_hw_str() {
    cli_send_str( "HW information is not available when not in CPU mode\n" );
}
#else
void cli_show_hw() {
    cli_send_str( "HW State:\n" );
    cli_show_hw_about();
    cli_show_hw_arp();
    cli_show_hw_intf();
    cli_show_hw_route();
    cli_show_hw_nat();
}

void cli_show_hw_about() {
    cli_printf("\nCompilation Time: %s %s\n", __DATE__, __TIME__);
}

void cli_show_hw_arp() {
    int i;
    struct nf2device* device = &ROUTER(get_sr())->device;
    uint32_t mac_hi,mac_lo,ip;
    uint8_t mac[ETHER_ADDR_LEN];
    cli_printf("\nARP table:\n");
    for(i = 0; i < ROUTER_OP_LUT_ARP_TABLE_DEPTH; i++)
    {
        writeReg(device, ROUTER_OP_LUT_ARP_TABLE_RD_ADDR, i);
        readReg(device, ROUTER_OP_LUT_ARP_TABLE_ENTRY_MAC_LO, &mac_lo);
        readReg(device, ROUTER_OP_LUT_ARP_TABLE_ENTRY_MAC_HI, &mac_hi);
        readReg(device, ROUTER_OP_LUT_ARP_TABLE_ENTRY_NEXT_HOP_IP, &ip);
        if((mac_lo == 0) && (mac_hi == 0))
        {
            break;
        }
        *((uint32_t *)(mac+2)) = htonl(mac_lo);
        *((uint16_t *)(mac)) = htonl(mac_hi) >> 16;
        print_mac(mac,cli_printf);
        cli_printf("\t");
        print_ip(htonl(ip),cli_printf);
        cli_printf("\n");
    }
}

void cli_show_hw_nat()
{
    int i;
    struct nf2device* device = &ROUTER(get_sr())->device;
    uint32_t out_ip,out_port,dst_ip,dst_port,src_ip,src_port;    

    cli_printf("\nNAT table:\n");
    for(i = 0; i < NAT_TABLE_DEPTH; i++)
    {
        writeReg(device, NAT_TABLE_RD_ADDR, i);

        readReg(device, NAT_TABLE_NAT_OUT_IP, &out_ip);
        readReg(device, NAT_TABLE_NAT_OUT_PORT, &out_port);
        readReg(device, NAT_TABLE_IP_DST, &dst_ip);
        readReg(device, NAT_TABLE_DST_PORT, &dst_port);
        readReg(device, NAT_TABLE_IP_SRC, &src_ip);
        readReg(device, NAT_TABLE_SRC_PORT, &src_port);

        src_ip = htonl(src_ip);
        out_ip = htonl(out_ip);
        dst_ip = htonl(dst_ip);

        if(out_ip != 0)
        {
            print_ip(htonl(src_ip),cli_printf);cli_printf(":0x%04x ",src_port);
            print_ip(htonl(out_ip),cli_printf);cli_printf(":0x%04x ",out_port);
            print_ip(htonl(dst_ip),cli_printf);cli_printf(":0x%04x ",dst_port);
            
            cli_printf("\n");
        }
    }    
}


static uint32_t interface_list_mac_hi[4] = { ROUTER_OP_LUT_MAC_0_HI,
                                             ROUTER_OP_LUT_MAC_1_HI,
                                             ROUTER_OP_LUT_MAC_2_HI,
                                             ROUTER_OP_LUT_MAC_3_HI };
static uint32_t interface_list_mac_lo[4] = { ROUTER_OP_LUT_MAC_0_LO,
                                             ROUTER_OP_LUT_MAC_1_LO,
                                             ROUTER_OP_LUT_MAC_2_LO,
                                             ROUTER_OP_LUT_MAC_3_LO };


void cli_show_hw_intf()
{
    int i;
    uint32_t mac_hi,mac_lo,ip;
    uint8_t mac[ETHER_ADDR_LEN];
    struct nf2device* device = &ROUTER(get_sr())->device;
    cli_printf("\nMAC Addresses:\n");
    for(i = 0; i < 4; i++)
    {
        cli_printf("%d: ",i);
        readReg(device, interface_list_mac_lo[i], &mac_lo);
        readReg(device, interface_list_mac_hi[i], &mac_hi);
        *((uint32_t *)(mac+2)) = htonl(mac_lo);
        *((uint16_t *)(mac)) = htonl(mac_hi) >> 16;
        print_mac(mac,cli_printf);cli_printf("\n");
    }
    cli_printf("\nIP Filter Table:\n");
 
    for (i = 0; i < ROUTER_OP_LUT_DST_IP_FILTER_TABLE_DEPTH; i++)
    {
        writeReg(device, ROUTER_OP_LUT_DST_IP_FILTER_TABLE_RD_ADDR, i);
        readReg(device, ROUTER_OP_LUT_DST_IP_FILTER_TABLE_ENTRY_IP, &ip);
        ip = htonl(ip);
        if(ip != 0)
        {
            print_ip(ip,cli_printf);cli_printf("\n");
        }
    }
}

void cli_show_hw_route() {
    int i;
    uint32_t ip, mask, next_hop, port;
    char * name;
    struct nf2device* device = &ROUTER(get_sr())->device;
    cli_printf("\nRouting Table:\n");

    for(i = 0; i < ROUTER_OP_LUT_ROUTE_TABLE_DEPTH; i++)
    {
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_RD_ADDR, i);
        readReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_IP, &ip);
        readReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_MASK, &mask);
        readReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_NEXT_HOP_IP, &next_hop);
        readReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_OUTPUT_PORT, &port);
        if(port != 0)
        {
            print_ip(htonl(ip),cli_printf);cli_printf("   ");
            print_ip(htonl(mask),cli_printf);cli_printf("   ");
            print_ip(htonl(next_hop),cli_printf);cli_printf("   ");
            name = interface_list_get_ifname_from_port(get_sr(),port);
            cli_printf("  %s (%d)\n",name ? name : "", port);
        }
    }
}
#endif

void cli_show_ip() {
    cli_print_rid();
    cli_show_ip_arp();
    cli_show_ip_intf();
    cli_show_ip_route();
    cli_show_nat();
}

void cli_show_nat()
{
    nat_show(get_sr(),cli_printf);
}

void cli_show_ip_arp() {
    arp_cache_show(ROUTER(get_sr())->a_cache, cli_printf);
}

void cli_show_ip_intf() {
    interface_list_show(ROUTER(get_sr())->iflist, cli_printf);
}

void cli_show_ip_route() {
    forwarding_table_dynamic_show(FORWARDING_TABLE(get_sr()),cli_printf);
    forwarding_table_static_show(FORWARDING_TABLE(get_sr()),cli_printf);
}

void cli_show_opt() {
    cli_show_opt_verbose();
}

void cli_show_opt_verbose() {
    if( *pverbose )
        cli_send_str( "Verbose: Enabled\n" );
    else
        cli_send_str( "Verbose: Disabled\n" );
}

void cli_show_ospf() {
    cli_print_rid();
    cli_send_str( "Neighbor Information:\n" );
    cli_show_ospf_neighbors();

    cli_send_str( "Topology:\n" );
    cli_show_ospf_topo();
}

void cli_show_ospf_neighbors() {
    interface_list_show_neighbours(INTERFACE_LIST(get_sr()),cli_printf);
}

void cli_show_ospf_topo() {
    link_state_graph_show_topology(LSG(get_sr()),cli_printf);
}

#ifndef _VNS_MODE_
void cli_send_no_vns_str() {
#ifdef _CPUMODE_
    cli_send_str( "VNS information is not available when in CPU mode\n" );
#else
    cli_send_str( "VNS information is not available when in Manual mode\n" );
#endif
}
#else
void cli_show_vns() {
    cli_send_str( "VNS State:\n  Localhost: " );
    cli_show_vns_lhost();
    cli_send_str( "  Topology: " );
    cli_show_vns_topo();
    cli_send_str( "  User: " );
    cli_show_vns_user();
    cli_send_str( "  Virtual Host: " );
    cli_show_vns_vhost();
}

void cli_show_vns_lhost() {
    cli_send_strln( SR->lhost );
}

void cli_show_vns_topo() {
    char buf[7];
    snprintf( buf, 7, "%u\n", SR->topo_id );
    cli_send_str( buf );
}

void cli_show_vns_user() {
    cli_send_strln( SR->user );
}

void cli_show_vns_vhost() {
    cli_send_strln( SR->vhost );
}
#endif

void cli_manip_ip_arp_add( gross_arp_t* data ) {
    char ip[STRLEN_IP];
    char mac[STRLEN_MAC];

    ip_to_string( ip, data->ip );
    mac_to_string( mac, data->mac );

    if( arp_cache_static_entry_add( SR, data->ip, data->mac ) )
        cli_send_strs( 5, "Added translation of ", ip, " <-> ", mac, " to the static ARP cache\n" );
    else
        cli_send_strs( 5, "Error: Unable to add a translation of ", ip, " <-> ", mac,
                       " to the static ARP cache -- try removing another static entry first.\n" );
}

void cli_manip_ip_arp_del( gross_arp_t* data ) {
    char ip[STRLEN_IP];

    ip_to_string( ip, data->ip );
    if( arp_cache_static_entry_remove( SR, data->ip ) )
        cli_send_strs( 3, "Removed ", ip, " from the ARP cache\n" );
    else
        cli_send_strs( 3, "Error: ", ip, " was not a static ARP cache entry\n" );
}

void cli_manip_ip_arp_purge_all() {
    int countD, countS, countT;
    char str_countS[11];
    char str_countT[11];
    const char* whatS;
    const char* whatT;

    countD = arp_cache_dynamic_purge( SR );

    countS = arp_cache_static_purge( SR );
    whatS = ( countS == 1 ) ? " entry" : " entries";
    snprintf( str_countS, 11, "%u", countS );

    countT = countD + countS;
    whatT = ( countT == 1 ) ? " entry" : " entries";
    snprintf( str_countT, 11, "%u", countT );

    cli_send_strs( 8, "Removed ", str_countT, whatT,
                   " (", str_countS, " static", whatS, ") from the ARP cache\n" );
}

void cli_manip_ip_arp_purge_dyn() {
    int count;
    char str_count[11];
    const char* what;

    count = arp_cache_dynamic_purge( SR );
    what = ( count == 1 ) ? " entry" : " entries";
    snprintf( str_count, 11, "%u", count );
    cli_send_strs( 4, "Removed ", str_count, what, " from the ARP cache\n" );
}

void cli_manip_ip_arp_purge_sta() {
    int count;
    char str_count[11];
    const char* what;

    count = arp_cache_static_purge( SR );
    what = ( count == 1 ) ? " entry" : " entries";
    snprintf( str_count, 11, "%u", count );
    cli_send_strs( 5, "Removed ", str_count, " static", what, " from the ARP cache\n" );
}

void cli_manip_ip_intf_set( gross_intf_t* data ) {
    void* intf;
    intf = router_lookup_interface_via_name( SR, data->intf_name );
    if( intf ) {
        interface_list_set_ip(SR, data->intf_name, data->ip, data->subnet_mask);
    }
    else
        cli_send_strs( 2, data->intf_name, " is not a valid interface\n" );
}

void cli_manip_ip_intf_set_enabled( const char* intf_name, int enabled ) {
    int ret;
    const char* what;

    ret = router_interface_set_enabled( SR, intf_name, enabled );
    what = (enabled ? "enabled\n" : "disabled\n");

    switch( ret ) {
    case 0:
        cli_send_strs( 3, intf_name, " has been ", what );
        break;

    case 1:
        cli_send_strs( 3, intf_name, " was already ", what );

    case -1:
    default:
        cli_send_strs( 2, intf_name, " is not a valid interface\n" );
    }
}

void cli_manip_ip_intf_down( gross_intf_t* data ) {
    cli_manip_ip_intf_set_enabled( data->intf_name, 0 );
}

void cli_manip_ip_intf_up( gross_intf_t* data ) {
    cli_manip_ip_intf_set_enabled( data->intf_name, 1 );
}

void cli_manip_ip_ospf_down() {
    if( router_is_ospf_enabled( SR ) ) {
        router_set_ospf_enabled( SR, 0 );
        cli_send_str( "OSPF has been disabled" );
    }
    else
        cli_send_str( "OSPF was already disabled" );
}

void cli_manip_ip_ospf_up() {
    if( !router_is_ospf_enabled( SR ) ) {
        router_set_ospf_enabled( SR, 1 );
        cli_send_str( "OSPF has been enabled" );
    }
    else
        cli_send_str( "OSPF was already enabled" );
}

void cli_manip_ip_route_add( gross_route_t* data ) {
    if( !interface_list_get_MAC_and_IP_from_name(INTERFACE_LIST(get_sr()),(char*)data->intf_name,0,0) )
        cli_send_strs( 3, "Error: no interface with the name ",
                       data->intf_name, " exists.\n" );
    else {
        rtable_route_add(SR, data->dest, data->gw, data->mask, (char *)data->intf_name, 1);
        cli_send_str( "The route has been added.\n" );
    }
}

void cli_manip_ip_route_del( gross_route_t* data ) {
    if( rtable_route_remove( SR, data->dest, data->mask, 1 ) )
        cli_send_str( "The route has been removed.\n" );
    else
        cli_send_str( "That route does not exist.\n" );
}

void cli_manip_ip_route_purge_all() {
    rtable_purge_all( SR );
    cli_send_str( "All routes have been removed from the routing table.\n" );
}

void cli_manip_ip_route_purge_dyn() {
    rtable_purge( SR, 0 );
    cli_send_str( "All dymanic routes have been removed from the routing table.\n" );
}

void cli_manip_ip_route_purge_sta() {
    rtable_purge( SR, 1 );
    cli_send_str( "All static routes have been removed from the routing table.\n" );
}

void cli_date() {
    char str_time[STRLEN_TIME];
    struct timeval now;

    gettimeofday( &now, NULL );
    time_to_string( str_time, now.tv_sec );
    cli_send_str( str_time );
}

void cli_exit() {
    cli_send_str( "Goodbye!\n" );
    fd_alive = 0;
}

int cli_ping_handle_self( uint32_t ip ) {
    void* intf;

    intf = router_lookup_interface_via_ip( SR, ip );
    if( intf ) {
        if( router_is_interface_enabled( SR, intf ) )
            cli_send_str( "Your interface is up.\n" );
        else
            cli_send_str( "Your interface is down.\n" );

        return 1;
    }

    return 0;
}

static int ping_outstanding = 0;
static int trace_outstanding = 0;

static int traceroute_seq;
static uint32_t traceroute_ip;


/**
 * Sends a ping to the specified IP address.  Information about it being sent
 * and whether it succeeds or not should be sent to the specified client_fd.
 */
static void cli_send_ping( int client_fd, uint32_t ip ) {
    ping_outstanding = 1;
    icmp_send_ping(get_sr(),ip,0,0,63);
}

void cli_dest_unreach(struct icmphdr * icmp, uint32_t ip)
{
    if(ping_outstanding || trace_outstanding)
    {
        cli_printf("destination "); print_ip(ip,cli_printf);
        cli_printf(" unreachable\n");
        cli_send_prompt();
        ping_outstanding = 0;
        trace_outstanding = 0;
    }
}


void cli_traceroute_print(uint32_t ip, int seq)
{
    cli_printf("%d:  ",seq-1);print_ip(ip,cli_printf);
    cli_printf("\n");
}

void cli_ping_reply(uint32_t ip)
{
    if(ping_outstanding)
    {
        cli_printf("Ping from ");
        print_ip(ip,cli_printf);
        cli_printf("\n");
        cli_send_prompt();
        ping_outstanding = 0;
    }
    if(trace_outstanding)
    {
        cli_traceroute_print(ip,traceroute_seq);
        cli_send_prompt();
        trace_outstanding = 0;
    }
}

void cli_ping( gross_ip_t* data ) {
    if( cli_ping_handle_self( data->ip ) )
        return;

    cli_send_ping(fd, data->ip );
    skip_next_prompt = 1;
}

void cli_ping_flood( gross_ip_int_t* data ) {
    int i;
    char str_ip[STRLEN_IP];

    if( cli_ping_handle_self( data->ip ) )
        return;

    ip_to_string( str_ip, data->ip );
    if( 0 != writenf( fd, "Will ping %s %u times ...\n", str_ip, data->count ) )
        fd_alive = 0;

    for( i=0; i<data->count; i++ )
        cli_send_ping( fd, data->ip );
    skip_next_prompt = 1;
}

void cli_shutdown() {
    cli_send_str( "Shutting down the router ...\n" );
    router_shutdown = 1;
    sr_integ_destroy(get_sr());

    /* we could do a cleaner shutdown, but this is probably fine */
    exit(0);
}

void cli_traceroute( gross_ip_t* data )
{
    trace_outstanding = 1;
    traceroute_seq = 2;
    traceroute_ip = data->ip;
    icmp_send_ping(get_sr(), data->ip, 1, 0, traceroute_seq);
    skip_next_prompt = 1;
}


void cli_time_exceeded(struct icmphdr * icmp, uint32_t ip)
{
    if(trace_outstanding)
    {
        cli_traceroute_print(ip,traceroute_seq);
        traceroute_seq++;
        icmp_send_ping(get_sr(), traceroute_ip, 1, 0, traceroute_seq);
    }
}


void cli_opt_verbose( gross_option_t* data ) {
    if( data->on ) {
        if( *pverbose )
            cli_send_str( "Verbose mode is already enabled.\n" );
        else {
            *pverbose = 1;
            cli_send_str( "Verbose mode is now enabled.\n" );
        }
    }
    else {
        if( *pverbose ) {
            *pverbose = 0;
            cli_send_str( "Verbose mode is now disabled.\n" );
        }
        else
            cli_send_str( "Verbose mode is already disabled.\n" );
    }
}
