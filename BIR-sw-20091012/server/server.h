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
    void server_update_arptable();
    void server_update_arptable_s();

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
    void update_arptable(int);

    static struct sr_instance * SR;
    static TestServer * server;
protected:
    static set<TestApp * > _updateList;
    static struct sr_mutex * _updateMutex;
};

#endif


#endif
