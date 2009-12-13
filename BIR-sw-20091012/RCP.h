#ifndef __RCP_H
#define __RCP_H

#include "mutex.h"

struct sr_instance;
struct RCPServer;

struct RCPClient
{
    int clientfd;
    struct RCPServer * server;
};

struct RCPServer
{
    struct sr_instance * sr;
    struct sr_mutex * mutex;
    int port;
    int sockfd;
    struct assoc_array * clients;
};

struct RCPData
{
    int code;
    int nbytes;
    char * data;
};

struct RCPServer * RCPSeverCreate(struct sr_instance * sr, int port);
void RCPServerDestroy(struct RCPServer * rcp);

void RCPHandleRequest(struct sr_instance * sr, struct RCPData * data);
void RCPError(struct sr_instance * sr, struct RCPData * data, char * message);

#endif
