/*-----------------------------------------------------------------------------
 * File: sr_base.c
 * Date: Spring 2002
 * Author: Martin Casado <casado@stanford.edu>
 *
 * Entry module to the low level networking subsystem of the router.
 *
 * Caveats:
 *
 *  - sys_thread_init(..) in sr_lwip_transport_startup(..) MUST be called from
 *    the main thread before other threads have been started.
 *
 *  - lwip requires that only one instance of the IP stack exist, therefore
 *    at the moment we don't support multiple instances of sr.  However
 *    support for this (given a cooperative tcp stack) would be simple,
 *    simple allow sr_init_low_level_subystem(..) to create new sr_instances
 *    each time they are called and return an identifier.  This identifier
 *    must be passed into sr_global_instance(..) to return the correct
 *    instance.
 *
 *  - lwip needs to keep track of all the threads so we use its
 *    sys_thread_new(), this is essentially a wrapper around
 *    pthread_create(..) that saves the thread's ID.  In the future, if
 *    we move away from lwip we should simply use pthread_create(..)
 *
 *
 *---------------------------------------------------------------------------*/

#ifdef _SOLARIS_
#define __EXTENSIONS__
#endif /* _SOLARIS_ */

/* get unistd.h to declare gethostname on linux */
#define __USE_BSD 1

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#ifdef _LINUX_
#include <getopt.h>
#endif /* _LINUX_ */

#include "lwip/tcp.h"
#include "lwip/memp.h"
#include "lwip/transport_subsys.h"

#include "sr_vns.h"
#include "sr_base_internal.h"

#ifdef _CPUMODE_
#include "sr_cpu_extension_nf2.h"
#endif

#include "sr_rt.h"
#include "../common.h"

extern char* optarg;

static void usage(char* );
static int  sr_lwip_transport_startup(void);
static void sr_set_user(struct sr_instance* sr);
static void sr_init_instance(struct sr_instance* sr, struct sr_options *);
static void sr_low_level_network_subsystem(void *arg);
static void sr_destroy_instance(struct sr_instance* sr);


/** run the command-line interface on CLI_PORT */
#define CLI_PORT 2300


/*----------------------------------------------------------------------------
 * sr_init_low_level_subystem
 *
 * Entry method to the sr low level network subsystem. Responsible for
 * managing, connecting to the server, reserving the topology, reading
 * the hardware information and starting the packet recv(..) loop in a
 * seperate thread.
 *
 * Caveats :
 *  - this method can only be called once!
 *
 *---------------------------------------------------------------------------*/

int sr_init_low_level_subystem(int argc, char **argv)
{
    /* -- VNS default parameters -- */
    char  *host   = "vrhost";
    char  *rtable = "rtable";
    char  *server = "171.67.71.18";
    uint16_t port =  12345;
    uint16_t topo =  0;

    char  *client = 0;
    char  *logfile = 0;

    char * cpu_hw_file_name = CPU_HW_FILENAME;
    int ret = CLI_PORT;

    /* -- singleton instance of router, passed to sr_get_global_instance
          to become globally accessible                                  -- */
    static struct sr_instance* sr = 0;
    struct sr_options opt;
    int option_index = 0;
    
    const struct option long_options[] = {
        { "arp_proxy"  ,   1, &opt.arp_proxy, 0 },
        { "aid"        ,   1, &opt.aid, 0 },
        { "debug_show" ,   1, &opt.debug_show, 0 },
        { "RCPPort"    ,   1, &opt.RCPPort, 0 },        
        { NULL         ,   0, NULL, 0 }
    };
    
    sr_router_default_options(&opt);

    int c;

    if ( sr )
    {
        fprintf(stderr,  "Warning: because of limitations in lwip, ");
        fprintf(stderr,  " sr supports 1 router instance per process. \n ");
        return 1;
    }

    sr = (struct sr_instance*) malloc(sizeof(struct sr_instance));
    sr_get_global_instance(sr); /* actually *sets* global instance! */

    while ((c = getopt_long(argc, argv, "hs:v:p:P:c:t:r:l:i:", long_options, &option_index)) != EOF)
    {
        if(c == -1)
        {
            usage("./sr");
            exit(0);
        }
        switch (c)
        {
        case 0:
            if(optarg)
            {
                *(long_options[option_index].flag) = atoi((char *) optarg);
            }
            break;
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'p':
                port = atoi((char *) optarg);
                break;
            case 'P':
                ret = atoi((char *) optarg);
                break;
            case 't':
                topo = atoi((char *) optarg);
                break;
            case 'v':
                host = optarg;
                break;
            case 'r':
                rtable = optarg;
                break;
            case 'c':
                client = optarg;
                break;
            case 's':
                server = optarg;
                break;
            case 'l':
                logfile = optarg;
                break;
            case 'i':
                cpu_hw_file_name = optarg;
                break;            
        } /* switch */
    } /* -- while -- */

#ifdef _CPUMODE_
    Debug(" \n ");
    Debug(" < -- Starting sr in cpu mode -- >\n");
    Debug(" \n ");
#else
    Debug(" < -- Starting sr in router mode  -- >\n");
    Debug(" \n ");
#endif /* _CPUMODE_ */

    /* -- required by lwip, must be called from the main thread -- */
    sys_thread_init();

    /* -- zero out sr instance and set default configurations -- */
    sr_init_instance(sr,&opt);

    /* -- set up routing table from file -- */
    if(sr_load_rt(sr->interface_subsystem, rtable) != 0)
    {
        fprintf(stderr,"Error setting up routing table from file %s\n",
                rtable);
        exit(1);
    }


    printf("Loading routing table\n");
    printf("---------------------------------------------\n");
    sr_print_routing_table(sr->interface_subsystem);
    printf("---------------------------------------------\n");

    
#ifdef _CPUMODE_
    sr->topo_id = 0;
    strncpy(sr->vhost,  "cpu",    SR_NAMELEN);
    strncpy(sr->rtable, rtable, SR_NAMELEN);

    if ( sr_cpu_init_hardware(sr, cpu_hw_file_name) )
    { exit(1); }
    sr_integ_hw_setup(sr);
#else
    sr->topo_id = topo;
    strncpy(sr->vhost,  host,    SR_NAMELEN);
    strncpy(sr->rtable, rtable, SR_NAMELEN);
#endif /* _CPUMODE_ */

    if(! client )
    { sr_set_user(sr); }
    else
    { strncpy(sr->user, client,  SR_NAMELEN); }

    if ( gethostname(sr->lhost,  SR_NAMELEN) == -1 )
    {
        perror("gethostname(..)");
        return 1;
    }

    /* -- log all packets sent/received to logfile (if non-null) -- */
    sr_vns_init_log(sr, logfile);

    sr_lwip_transport_startup();


#ifndef _CPUMODE_
    Debug("Client %s connecting to Server %s:%d\n",
            sr->user, server, port);
    Debug("Requesting topology %d\n", topo);

    /* -- connect to VNS and reserve host -- */
    if(sr_vns_connect_to_server(sr,port,server) == -1)
    { return 1; }

    /* read from server until the hardware is setup */
    while (! sr->hw_init )
    {
        if(sr_vns_read_from_server(sr) == -1 )
        {
            fprintf(stderr, "Error: could not get hardware information from the server\n");
            sr_destroy_instance(sr);
            return 1;
        }
    }
#endif

    /* -- start low-level network thread, dissown sr -- */
    sys_thread_new(sr_low_level_network_subsystem, (void*)sr /* dissown */);

    return ret;
}/* -- main -- */

/*-----------------------------------------------------------------------------
 * Method: sr_set_subsystem(..)
 * Scope: Global
 *
 * Set the router core in sr_instance
 *
 *---------------------------------------------------------------------------*/

void sr_set_subsystem(struct sr_instance* sr, void* core)
{
    sr->interface_subsystem = core;
} /* -- sr_set_subsystem -- */


/*-----------------------------------------------------------------------------
 * Method: sr_get_subsystem(..)
 * Scope: Global
 *
 * Return the sr router core
 *
 *---------------------------------------------------------------------------*/

struct sr_ifsys *sr_get_subsystem(struct sr_instance* sr)
{
    return sr->interface_subsystem;
} /* -- sr_get_subsystem -- */

/*-----------------------------------------------------------------------------
 * Method: sr_get_global_instance(..)
 * Scope: Global
 *
 * Provide the world with access to sr_instance(..)
 *
 *---------------------------------------------------------------------------*/

struct sr_instance* sr_get_global_instance(struct sr_instance* sr)
{
    static struct sr_instance* sr_global_instance = 0;

    if ( sr )
    { sr_global_instance = sr; }

    return sr_global_instance;
} /* -- sr_get_global_instance -- */

/*-----------------------------------------------------------------------------
 * Method: sr_low_level_network_subsystem(..)
 * Scope: local
 *---------------------------------------------------------------------------*/

static void sr_low_level_network_subsystem(void *arg)
{
    struct sr_instance* sr = (struct sr_instance*)arg;

    /* -- NOTE: the argument has *already* been set as global singleton -- */


#ifdef _CPUMODE_
    /* -- whizbang main loop ;-) */
    while( sr_cpu_input(sr) == 1);
#else
    /* -- whizbang main loop ;-) */
    while( sr_vns_read_from_server(sr) == 1);
#endif

   /* -- this is the end ... my only friend .. the end -- */
    sr_destroy_instance(sr);
} /* --  sr_low_level_network_subsystem -- */

/*-----------------------------------------------------------------------------
 * Method: sr_lwip_transport_startup(..)
 * Scope: local
 *---------------------------------------------------------------------------*/

static int sr_lwip_transport_startup(void)
{
    sys_init();
    mem_init();
    memp_init();
    pbuf_init();

    transport_subsys_init(0, 0);

    return 0;
} /* -- sr_lwip_transport_startup -- */


/*-----------------------------------------------------------------------------
 * Method: sr_set_user(..)
 * Scope: local
 *---------------------------------------------------------------------------*/

static void sr_set_user(struct sr_instance* sr)
{
    uid_t uid = getuid();
    struct passwd* pw = 0;

    /* REQUIRES */
    assert(sr);

    if(( pw = getpwuid(uid) ) == 0)
    {
        fprintf (stderr, "Error getting username, using something silly\n");
        strncpy(sr->user, "something_silly",  SR_NAMELEN);
    }
    else
    { strncpy(sr->user, pw->pw_name,  SR_NAMELEN); }

} /* -- sr_set_user -- */

/*-----------------------------------------------------------------------------
 * Method: sr_init_instance(..)
 * Scope:  Local
 *
 *----------------------------------------------------------------------------*/

static
void sr_init_instance(struct sr_instance* sr, struct sr_options * opt)
{
    /* REQUIRES */
    assert(sr);

    sr->sockfd   = -1;
    sr->user[0]  = 0;
    sr->vhost[0] = 0;
    sr->topo_id  = 0;
    sr->logfile  = 0;
    sr->hw_init  = 0;

    sr->interface_subsystem = 0;

    pthread_mutex_init(&(sr->send_lock), 0);

    sr_integ_init(sr,opt);
} /* -- sr_init_instance -- */

/*-----------------------------------------------------------------------------
 * Method: sr_destroy_instance(..)
 * Scope:  local
 *
 *----------------------------------------------------------------------------*/

static void sr_destroy_instance(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    sr_integ_destroy(sr);
} /* -- sr_destroy_instance -- */

/*-----------------------------------------------------------------------------
 * Method: usage(..)
 * Scope: local
 *---------------------------------------------------------------------------*/

static void usage(char* argv0)
{
    printf("Simple Router Client\n Compile Time: (%s %s)\n", __TIME__, __DATE__);
    printf("Format: %s [-h] [-v host] [-s server] [-p VNS port] \n",argv0);
    printf("           [-t topo id] [-P cli port] [-i cpu config file]\n");
    printf("\nLong Options:\n");
    printf("  --arp_proxy   (=1,0) arp proxying (on,off)\n");
    printf("  --aid router  area id\n");
    printf("  --debug_show  (=1,0) dump packet (on,off)\n");
    printf("  --RCPPort     Port for RCP Server\n");
} /* -- usage -- */
