/**
 * Filename: cli_stus.h
 * Author: David Underhill
 * Purpose: TEMPORARY file where router hooks are defined temporarily until they
 *          are implemented for real.
 */

#ifndef CLI_STUBS_H
#define CLI_STUBS_H


/**
 * Add a static entry to the static ARP cache.
 * @return 1 if succeeded (fails if the max # of static entries are already
 *         in the cache).
 */
int arp_cache_static_entry_add( struct sr_instance* sr,
                                uint32_t ip,
                                uint8_t* mac ) {
    arp_cache_add(sr,ip,mac,0);
    return 1; 
}

/**
 * Remove a static entry to the static ARP cache.
 * @return 1 if succeeded (false if ip wasn't in the cache as a static entry)
 */
int arp_cache_static_entry_remove( struct sr_instance* sr, uint32_t ip ) {
    return arp_cache_remove_entry(sr,ip,0);
}

/**
 * Remove all static entries from the ARP cache.
 * @return  number of static entries removed
 */
unsigned arp_cache_static_purge( struct sr_instance* sr ) {
    return arp_cache_purge(sr,0);
}

/**
 * Remove all dynamic entries from the ARP cache.
 * @return  number of dynamic entries removed
 */
unsigned arp_cache_dynamic_purge( struct sr_instance* sr ) {
    return arp_cache_purge(sr,1);
}

/**
 * Enables or disables an interface on the router.
 * @return 0 if name was enabled
 *         -1 if it does not not exist
 *         1 if already set to enabled
 */
int router_interface_set_enabled( struct sr_instance* sr, const char* name, int enabled ) {
    return interface_list_set_enabled(sr,(char *)name,enabled);
}

/**
 * Returns a pointer to the interface which is assigned the specified IP.
 *
 * @return interface, or NULL if the IP does not belong to any interface
 *         (you'll want to change void* to whatever type you end up using)
 */
void* router_lookup_interface_via_ip( struct sr_instance* sr,
                                      uint32_t ip ) {
    return interface_list_get_interface_by_ip(INTERFACE_LIST(sr), ip);
}

/**
 * Returns a pointer to the interface described by the specified name.
 *
 * @return interface, or NULL if the name does not match any interface
 *         (you'll want to change void* to whatever type you end up using)
 */
void* router_lookup_interface_via_name( struct sr_instance* sr,
                                        const char* name ) {
    char cpname[SR_NAMELEN];
    strcpy(cpname,name);
    return interface_list_get_interface_by_name(INTERFACE_LIST(sr), cpname);
}

/**
 * Returns 1 if the specified interface is up and 0 otherwise.
 */
int router_is_interface_enabled( struct sr_instance* sr, void* intf ) {
    return interface_list_interface_up(sr, intf);
}

/**
 * Returns whether OSPF is enabled (0 if disabled, otherwise it is enabled).
 */
int router_is_ospf_enabled( struct sr_instance* sr ) {
    return !OPTIONS(sr)->disable_ospf;
}

/**
 * Sets whether OSPF is enabled.
 */
void router_set_ospf_enabled( struct sr_instance* sr, int enabled ) {
    OPTIONS(sr)->disable_ospf = 0;
}

/** Adds a route to the appropriate routing table. */
void rtable_route_add( struct sr_instance* sr,
                       uint32_t dest, uint32_t gw, uint32_t mask,
                       char * intf,
                       int is_static_route ) {
    struct ip_address ip = {dest,mask};
    if(interface_list_outbound(sr, intf))
    {
        forwarding_table_add(sr,&ip,gw,intf,!is_static_route,1);
    }
    forwarding_table_add(sr,&ip,gw,intf,!is_static_route,0);
}

/** Removes the specified route from the routing table, if present. */
int rtable_route_remove( struct sr_instance* sr,
                         uint32_t dest, uint32_t mask,
                         int is_static ) {
    struct ip_address ip = {dest,mask};
    forwarding_table_remove(sr,&ip,!is_static,1);
    return forwarding_table_remove(sr,&ip,!is_static,0);
}

/** Remove all routes from the router. */
void rtable_purge_all( struct sr_instance* sr ) {
    fprintf( stderr, "not yet implemented: rtable_purge_all\n" );
}

/** Remove all routes of a specific type from the router. */
void rtable_purge( struct sr_instance* sr, int is_static ) {
    fprintf( stderr, "not yet implemented: rtable_purge\n" );
}

#endif /* CLI_STUBS_H */
