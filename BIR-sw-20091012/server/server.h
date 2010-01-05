#ifndef _SERVER_H
#define _SERVER_H

struct sr_instance;

#ifdef CPP
extern "C" {
#endif

    void runserver(struct sr_instance * sr);
    void stopserver();
    void server_update_fwdtable();
    void server_update_fwdtable_s();
    void server_update_arptable(int ttl);
    void server_update_arptable_s();
    void server_update_ip();
    void server_update_router_ttl();

#ifdef CPP
}
#endif


#ifdef CPP

#include <Wt/WApplication>
#include <Wt/WEnvironment>
#include <Wt/WContainerWidget>
#include <Wt/WServer>
#include <Wt/WText>
#include <Wt/WTable>
#include <iostream>
#include <string>
#include "../lwtcp/lwip/sys.h"
#include <set>
#include <Wt/WMessageBox>

using namespace Wt;
using namespace std;

class TestApp;

class TestServer : public WServer
{
    static WApplication * createApplication(const WEnvironment & env);
public:
    TestServer(struct sr_instance * sr, int argc, char ** argv);
    ~TestServer();
    static void AddToUpdateList(TestApp *);
    static void RemoveFromUpdateList(TestApp *);

    void update_fwdtable(int);
    void update_arptable(int,int);
    void update_ip();
    void update_router_ttl();

    static struct sr_instance * SR;
    static TestServer * server;

    static void ip_err()
    {
        WMessageBox::show("Error", "Invalid IP Entry."
                          "An IP Address must have the form X.X.X.X"
                          " where X is a string from 0123456789", Ok);
    }

    static void mac_err()
    {
        WMessageBox::show("Error", "Invalid MAC Entry."
                          "A MAC Address must have the form xx:xx:xx:xx:xx:xx"
                          " where x is one of 0123456789abcdef", Ok);
    }
protected:
    static set<TestApp * > _updateList;
    static struct sr_mutex * _updateMutex;
};

#endif


#endif
