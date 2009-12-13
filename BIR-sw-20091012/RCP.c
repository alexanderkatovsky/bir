/*
 * RCP = Router Communication Protocol
 *
 * [code of function, size of data in bytes, data]
 * 
 * */
#include <errno.h>
#include "router.h"
#include "lwtcp/lwip/sys.h"
#include <unistd.h>

void * __RCPGetClient(void * data)
{
    return data;
}


int __writen(int fd, const void* buf, unsigned n)
{
    size_t nleft;
    ssize_t nwritten;
    char* chbuf;

    chbuf = (char*)buf;
    nleft = n;
    while( nleft > 0 )
    {
        if((nwritten = send(fd, chbuf, nleft, 0)) < 0 && errno != EINTR)
        {
            return -1;
        }

        nleft -= nwritten;
        chbuf += nwritten;
    }

    return 0;
}


ssize_t __readn( int fd, void* buf, unsigned n )
{
    size_t nleft;
    ssize_t nread;
    char* chbuf;

    if(n == 0)
    {
        return 0;
    }

    chbuf = (char*)buf;
    nleft = n;
    while( nleft > 0 )
    {
        nread = recv(fd, chbuf, nleft,0);
        if(nread < 0  && errno != EINTR)
        {
            return -1; 
        }
        else if(nread == 0)
        {
            break;
        }
        nleft -= nread;
        chbuf += nread;
    }

    return (n - nleft);
}

struct RCPClient * RCPClientCreate(struct RCPServer * rcp, int clientfd)
{
    NEW_STRUCT(ret,RCPClient);
    ret->clientfd = clientfd;
    ret->server = rcp;
    return ret;
}

void RCPClientDestroy(void * data)
{
    struct RCPClient * client = (struct RCPClient *)data;
    mutex_lock(client->server->mutex);
    assoc_array_delete(client->server->clients,client);
    mutex_unlock(client->server->mutex);
    close(client->clientfd);
    free(client);
}

void RCPClientThread(void * data)
{
    struct RCPClient * client = (struct RCPClient *)data;
    struct RCPData rdata = {0,0,0};

    while(1)
    {
        if(__readn(client->clientfd,&rdata.code,4) != 4 || __readn(client->clientfd,&rdata.nbytes,4) != 4)
        {
            break;
        }
        rdata.data = malloc(rdata.nbytes);
        if(__readn(client->clientfd,rdata.data,rdata.nbytes) != rdata.nbytes)
        {
            RCPError(client->server->sr, &rdata, "Failed to read data");
        }
        else
        {
            RCPHandleRequest(client->server->sr, &rdata);
        }
        if(__writen(client->clientfd,&rdata.code,4) != 0 ||
           __writen(client->clientfd,&rdata.nbytes,4) != 0 ||
           __writen(client->clientfd,rdata.data,rdata.nbytes) != 0)
        {
            break;
        }
        if(rdata.data)
        {
            free(rdata.data);
        }
        rdata.data = 0;
    }
    if(rdata.data)
    {
        free(rdata.data);
    }
    RCPClientDestroy(client);
}

/*
 * Listens for clients on the port and for each spawns a new thread to handle it
 * */
void RCPServerThread(void * data)
{
    struct RCPServer * rcp = (struct RCPServer *)data;
    struct sockaddr_in addr;
    struct sockaddr client_addr;
    int clientfd;
    unsigned sock_len;
    struct RCPClient * client;

    rcp->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_port = htons(rcp->port);
    addr.sin_addr.s_addr = 0;
    memset(&(addr.sin_zero), 0, sizeof(addr.sin_zero));
    bind(rcp->sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr));

    listen(rcp->sockfd, 10);
    printf("\nRCP Server listening on port %d\n",rcp->port);

    while(1)
    {
        clientfd = accept(rcp->sockfd, &client_addr, &sock_len);
        if(clientfd == -1 && errno != EINTR)
        {
            break;
        }
        client = RCPClientCreate(rcp, clientfd);
        mutex_lock(client->server->mutex);
        assoc_array_insert(rcp->clients, client);
        mutex_unlock(client->server->mutex);
        sys_thread_new(RCPClientThread,client);
    }
}

struct RCPServer * RCPSeverCreate(struct sr_instance * sr, int port)
{
    NEW_STRUCT(ret,RCPServer);
    
    ret->sr = sr;
    ret->port = port;
    ret->sockfd = -1;
    ret->clients = assoc_array_create(__RCPGetClient, assoc_array_key_comp_int);
    ret->mutex = mutex_create();

    sys_thread_new(RCPServerThread,ret);

    return ret;
}

int RCPCloseClients(void * data, void * userdata)
{
    struct RCPClient * client = (struct RCPClient *)data;

    close(client->clientfd);
    return 0;
}

void RCPServerDestroy(struct RCPServer * rcp)
{
    shutdown(rcp->sockfd,SHUT_RDWR);
    close(rcp->sockfd);
    assoc_array_walk_array(rcp->clients,RCPCloseClients,0);
    while(!assoc_array_empty(rcp->clients))
    {
        usleep(10000);
    }
    assoc_array_delete_array(rcp->clients,0);
    mutex_destroy(rcp->mutex);
    free(rcp);
}
