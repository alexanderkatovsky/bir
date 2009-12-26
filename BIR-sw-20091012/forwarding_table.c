#include "forwarding_table.h"
#include <stdlib.h>
#include <string.h>
#include "router.h"
#include "reg_defines.h"

void * __forwarding_table_get_key_dest(void * data)
{
    return &((struct forwarding_table_entry *)data)->ip.subnet;
}

void * __forwarding_table_get_key_mask(void * data)
{
    return &((struct forwarding_table_subnet_list *)data)->mask;
}

void __delete_forwarding_table_subnet_list(void * data)
{
    free((struct forwarding_table_entry *)data);
}

void __delete_forwarding_table(void * data)
{
    struct forwarding_table_subnet_list * ftsl = (struct forwarding_table_subnet_list *)data;
    assoc_array_delete_array(ftsl->list,__delete_forwarding_table_subnet_list);
    free(ftsl);
}

void forwarding_table_create(struct sr_instance * sr)
{
    NEW_STRUCT(ret,forwarding_table);
    ROUTER(sr)->fwd_table = ret;
    ret->array_s = assoc_array_create(__forwarding_table_get_key_mask,assoc_array_key_comp_int);
    ret->array_d = assoc_array_create(__forwarding_table_get_key_mask,assoc_array_key_comp_int);
    ret->array_nat = assoc_array_create(__forwarding_table_get_key_mask,assoc_array_key_comp_int);    
    ret->mutex = mutex_create();
    ret->running_dijkstra = 0;
}

void forwarding_table_destroy(struct forwarding_table * fwd_table)
{
    mutex_destroy(fwd_table->mutex);
    assoc_array_delete_array(fwd_table->array_s,__delete_forwarding_table);
    assoc_array_delete_array(fwd_table->array_d,__delete_forwarding_table);
    assoc_array_delete_array(fwd_table->array_nat,__delete_forwarding_table);    
    free(fwd_table);    
}

struct __LPMSearch
{
    int found;
    uint32_t ip;
    uint32_t mask;
    uint32_t * next_hop;
    char * thru;
};

int __LPMSearchFn(void * data, void * user_data)
{
    struct __LPMSearch * srch = (struct __LPMSearch *) user_data;
    struct forwarding_table_subnet_list * dl = (struct forwarding_table_subnet_list *) data;
    struct forwarding_table_entry * te;
    int subnet = srch->ip & dl->mask;
    if((te = assoc_array_read(dl->list,&subnet)) != NULL)
    {    
        srch->found = 1;
        srch->mask = dl->mask;
        if(srch->next_hop)
        {
            *srch->next_hop = te->next_hop;
        }
        if(srch->thru)
        {
            memcpy(srch->thru,te->interface,SR_NAMELEN);
        }
    }
    return srch->found;
}

/*
 * Implementation of LPM (longest prefix match) search of forwarding table
 * */
int forwarding_table_lookup_next_hop(struct forwarding_table * fwd_table,
                                     uint32_t ip, uint32_t * next_hop, char * thru, int nat)
{
    struct __LPMSearch srch = {0,ip,0,next_hop,thru};
    mutex_lock(fwd_table->mutex);
    
    assoc_array_walk_array(nat ? fwd_table->array_nat : fwd_table->array_d, __LPMSearchFn,(void*)&srch);
    if(srch.found == 0 || srch.mask == 0)
    {
        srch.found = 0;
        assoc_array_walk_array(fwd_table->array_s,__LPMSearchFn,(void*)&srch);
    }
    if(next_hop && *next_hop == 0)
    {
        *next_hop = ip;
    }

    mutex_unlock(fwd_table->mutex);

    return srch.found;
}

int __forwarding_table_get_entry(struct forwarding_table * fwd_table,
                                 struct assoc_array * array, struct ip_address * ip,
                                 struct forwarding_table_subnet_list ** ftsl, struct forwarding_table_entry ** fte)
{
    mutex_lock(fwd_table->mutex);
    *fte = 0;
    *ftsl = (struct forwarding_table_subnet_list *) assoc_array_read(array,&ip->mask);
    if(*ftsl != NULL)
    {
        *fte = assoc_array_read((*ftsl)->list, &ip->subnet);
    }
    mutex_unlock(fwd_table->mutex);
    return (*fte != NULL);
}

int forwarding_table_dynamic_entry_exists(struct forwarding_table * ft, struct ip_address * ip, int nat)
{
    struct forwarding_table_subnet_list * ftsl;
    struct forwarding_table_entry * fte;
    return __forwarding_table_get_entry(ft, nat ? ft->array_nat : ft->array_d, ip, &ftsl, &fte);
}

int forwarding_table_static_entry_exists(struct forwarding_table * ft, struct ip_address * ip)
{
    struct forwarding_table_subnet_list * ftsl;
    struct forwarding_table_entry * fte;
    return __forwarding_table_get_entry(ft, ft->array_s, ip, &ftsl, &fte);
}

void forwarding_table_add(struct sr_instance * sr, struct ip_address * ip,
                          uint32_t nh, char * interface, int isDynamic, int nat)
{
    struct forwarding_table * ft = FORWARDING_TABLE(sr);
    struct forwarding_table_subnet_list * ftsl;
    struct forwarding_table_entry * fte;
    NEW_STRUCT(entry,forwarding_table_entry);
    uint32_t next_hop = nh;
    struct assoc_array * array;

    if(interface_list_ip_in_network_on_interface(sr,ip,interface))
    {
        next_hop = 0;
    }

    entry->ip = *ip;
    entry->ip.subnet &= ip->mask;
    entry->next_hop = next_hop;
    strcpy(entry->interface,interface);

    mutex_lock(ft->mutex);
    array = isDynamic ? (nat ? ft->array_nat : ft->array_d) : ft->array_s;

    if(__forwarding_table_get_entry(ft, array, ip, &ftsl, &fte))
    {
        free(fte);
    }
    if(ftsl == NULL)
    {
        ftsl = (struct forwarding_table_subnet_list *) malloc(sizeof(struct forwarding_table_subnet_list));
        ftsl->mask = ip->mask;
        ftsl->list = assoc_array_create(__forwarding_table_get_key_dest, assoc_array_key_comp_int);
        assoc_array_insert(array,ftsl);
    }
    assoc_array_insert(ftsl->list,entry);
    if(ft->running_dijkstra == 0)
    {
        forwarding_table_hw_write(sr);
    }

    mutex_unlock(ft->mutex);
}

void forwarding_table_start_dijkstra(struct sr_instance * sr)
{
    struct forwarding_table * fwd_table = FORWARDING_TABLE(sr);
    assoc_array_delete_array(fwd_table->array_d,__delete_forwarding_table);
    fwd_table->array_d = assoc_array_create(__forwarding_table_get_key_mask,assoc_array_key_comp_int);
    assoc_array_delete_array(fwd_table->array_nat,__delete_forwarding_table);
    fwd_table->array_nat = assoc_array_create(__forwarding_table_get_key_mask,assoc_array_key_comp_int);    
    fwd_table->running_dijkstra = 1;
    mutex_lock(fwd_table->mutex);
}

void forwarding_table_end_dijkstra(struct sr_instance * sr)
{
    struct forwarding_table * fwd_table = FORWARDING_TABLE(sr);
    forwarding_table_hw_write(sr);
    fwd_table->running_dijkstra = 0;
    mutex_unlock(fwd_table->mutex);
}

struct __forwarding_table_loop_i
{
    void * userdata;
    int finished;
    void (*fn)(uint32_t,uint32_t,uint32_t,char*,void*,int*);
};

int __forwarding_table_list_loop_a(void * data, void * userdata)
{
    struct forwarding_table_entry * fte = (struct forwarding_table_entry *)data;
    struct __forwarding_table_loop_i * li = (struct __forwarding_table_loop_i *)userdata;
    li->fn(fte->ip.subnet, fte->ip.mask, fte->next_hop, fte->interface, li->userdata, &li->finished);
    return li->finished;
}

int __forwarding_table_loop_a(void * data, void * userdata)
{
    struct forwarding_table_subnet_list * sl = (struct forwarding_table_subnet_list *)data;
    struct __forwarding_table_loop_i * li = (struct __forwarding_table_loop_i *)userdata;
    assoc_array_walk_array(sl->list,__forwarding_table_list_loop_a,userdata);
    return li->finished;
}

void forwarding_table_loop(struct forwarding_table * ft,
                           void (*fn)(uint32_t,uint32_t,uint32_t,char*,void*,int*),
                           void * userdata, int isDynamic, int nat)
{
    struct __forwarding_table_loop_i li = {userdata,0,fn};
    struct assoc_array * array = isDynamic ? (nat ? ft->array_nat : ft->array_d) : ft->array_s;
    mutex_lock(ft->mutex);
    assoc_array_walk_array(array, __forwarding_table_loop_a, &li);
    mutex_unlock(ft->mutex);
}


void __forwarding_table_show_a(uint32_t subnet, uint32_t mask, uint32_t next_hop,
                               char * interface, void * userdata, int * finished)
{
    print_t print = (print_t)userdata;

    print_ip(subnet,print);print("\t");
    print_ip(mask,print);print("\t");
    print_ip(next_hop,print);print("\t");
    print("%s\n",interface);
}

void forwarding_table_dynamic_show(struct forwarding_table * ft, print_t print, int nat)
{
    print("Dynamic Forwarding Table:\n");
    forwarding_table_loop(ft, __forwarding_table_show_a, print, 1, nat);
}

void forwarding_table_static_show(struct forwarding_table * ft, print_t print)
{
    print("Static Forwarding Table:\n");
    forwarding_table_loop(ft, __forwarding_table_show_a, print, 0,0);
}

#ifdef _CPUMODE_

struct __forwarding_table_hw_write_i
{
    int count;
    struct sr_instance * sr;
};

void __forwarding_table_hw_write_a(uint32_t subnet, uint32_t mask, uint32_t next_hop,
                             char * interface, void * userdata, int * finished)
{
    struct __forwarding_table_hw_write_i * hwi = (struct __forwarding_table_hw_write_i *)userdata;
    struct nf2device * device = &ROUTER(hwi->sr)->device;
    if(hwi->count >= ROUTER_OP_LUT_ROUTE_TABLE_DEPTH)
    {
        *finished = 1;
    }
    else
    {
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_IP, ntohl(subnet));
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_MASK, ntohl(mask));
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_NEXT_HOP_IP, ntohl(next_hop));
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_OUTPUT_PORT,
                 interface_list_get_output_port(hwi->sr, interface));
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_WR_ADDR, hwi->count);
        hwi->count += 1;
    }
}
#endif

void forwarding_table_hw_write(struct sr_instance * sr)
{
#ifdef _CPUMODE_
    struct __forwarding_table_hw_write_i  hwi = {0,sr};
    struct nf2device * device = &ROUTER(sr)->device;
    int i;
    forwarding_table_loop(FORWARDING_TABLE(sr), __forwarding_table_hw_write_a, &hwi,1);

    for(i = hwi.count; i < ROUTER_OP_LUT_ROUTE_TABLE_DEPTH; i++)
    {
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_IP, 0);
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_MASK, 0xffffffff);
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_NEXT_HOP_IP, 0);
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_ENTRY_OUTPUT_PORT, 0);
        writeReg(device, ROUTER_OP_LUT_ROUTE_TABLE_WR_ADDR, i);
    }
#endif
}
