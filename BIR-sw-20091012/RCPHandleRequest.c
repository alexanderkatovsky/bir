#include "router.h"
#include "RCPCodes.h"

void RCPConstructData(struct RCPData * rdata, int code, int datalen, char * data)
{
    if(rdata->data)
    {
        free(rdata->data);
    }
    rdata->data = 0;
    if(datalen > 0)
    {
        rdata->data = malloc(datalen);
        memcpy(rdata->data, data, datalen);
    }

    rdata->nbytes = datalen;
    rdata->code = code;
}

void RCPError(struct sr_instance * sr, struct RCPData * data, char * message)
{
    RCPConstructData(data, RCP_CODE_ERROR, strlen(message), message);
}

void RCPGetRouterID(struct sr_instance * sr, struct RCPData * data)
{
    RCPConstructData(data, RCP_CODE_GET_ROUTER_ID, 4, (char *)&ROUTER(sr)->rid);
}

struct RCPGetForwardingTable_i
{
    struct fifo * list;
    int len;
};

struct RCPGetForwardingTable_s
{
    uint32_t subnet;
    uint32_t mask;
    uint32_t next_hop;
    char interface[SR_NAMELEN];
};

void RCPGetForwardingTable_a(uint32_t subnet, uint32_t mask, uint32_t next_hop,
                             char * interface, void * userdata, int * finished)
{
    struct RCPGetForwardingTable_i * ti = (struct RCPGetForwardingTable_i *)userdata;
    NEW_STRUCT(ret, RCPGetForwardingTable_s);
    ret->subnet = subnet;
    ret->mask = mask;
    ret->next_hop = next_hop;
    strcpy(ret->interface, interface);
    ti->len += 12 + strlen(interface) + 1;
    fifo_push(ti->list,ret);
}

void RCPGetForwardingTable(struct sr_instance * sr, struct RCPData * data, int isdynamic)
{
    struct fifo * list = fifo_create();
    char * raw;
    struct RCPGetForwardingTable_s * ts;
    int i = 0;
    struct RCPGetForwardingTable_i ti = {list,0};
    forwarding_table_loop(FORWARDING_TABLE(sr),RCPGetForwardingTable_a,&ti,isdynamic);
    raw = malloc(ti.len);
    while((ts = fifo_pop(ti.list)))
    {
        memcpy(raw + i, (char*)&ts->subnet, 4);
        memcpy(raw + i + 4, (char*)&ts->mask, 4);
        memcpy(raw + i + 8, (char*)&ts->next_hop, 4);
        memcpy(raw + i + 12, ts->interface, strlen(ts->interface) + 1);
        i += 12 + strlen(ts->interface) + 1;
        free(ts);
    }
    fifo_delete(ti.list,0);
    RCPConstructData(data, RCP_CODE_GET_FORWARDING_TABLE, ti.len, raw);
    free(raw);
}

void RCPHandleRequest(struct sr_instance * sr, struct RCPData * data)
{
    char buf[255];
    switch(data->code)
    {
    case RCP_CODE_GET_ROUTER_ID:
        RCPGetRouterID(sr,data);
        break;
    case RCP_CODE_GET_FORWARDING_TABLE:
        RCPGetForwardingTable(sr,data,1);
        break;
    default:
        sprintf(buf,"invalid code %d", data->code);
        RCPError(sr, data, buf);
    }
}
