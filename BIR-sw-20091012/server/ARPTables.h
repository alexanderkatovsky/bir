#ifndef __ARP_TABLES_H
#define __ARP_TABLES_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.h"
#include <Wt/WCssStyleSheet>
#include <Wt/WPushButton>
#include <Wt/WLineEdit>
#include <Wt/WRegExpValidator>
#include <Wt/WMessageBox>
#include <Wt/WComboBox>
#include <Wt/WPanel>
#include <Wt/WBreak>
#include "../router.h"


class ARPTable : public WTable
{
    int _isDynamic;
    
    struct TableElt
    {
        uint32_t ip;
        int ttl;
        uint8_t MAC[ETHER_ADDR_LEN];
        TableElt(uint32_t i, uint8_t * M, int t) :
            ip(i), ttl(t)
        {
            memcpy(MAC, M, ETHER_ADDR_LEN);
        }
    };

    class PushButton : public WPushButton
    {
        int _i;
        ARPTable * _arp;
    public:
        PushButton(int i, ARPTable * arp) : _i(i), _arp(arp){}
        void delete_entry()
        {
            _arp->delete_row(_i);
        }
    };

    vector<TableElt> __arr;
    WLineEdit * __wt_ip;
    WLineEdit * __wt_mac;
    WLineEdit * __wt_ttl;

    void __update();
    void __updateTtl();

public:
    ARPTable(int isDynamic = 1) : _isDynamic(isDynamic)
    {
        setHeaderCount(1);
        Update(0);
    }

    static void l_arptable(uint32_t ip, uint8_t * MAC, int ttl, void * userdata)
    {
        ((vector<TableElt> *)userdata)->push_back(TableElt(ip, MAC, ttl));
    }

    void delete_row(int i)
    {
        arp_cache_remove_entry(TestServer::SR, __arr[i].ip, 0);
    }

    void add_entry();

    void Update(int ttl);
};

class ARPTables : public WContainerWidget
{
protected:
    ARPTable * __arp_d, * __arp_s;
    WPanel * __wpd, * __wps;
    int __dyn_update, __s_update;
public:
    ARPTables();

    void NeedUpdate(int dyn, int ttl)
    {
        if(dyn)
        {
            __dyn_update = 1;
        }
        else
        {
            __s_update = 1;
        }
    }

    void Update()
    {
        if(__dyn_update)
        {
            __arp_d->Update(0);
            __dyn_update = 0;
        }
        if(__s_update)
        {
            __arp_s->Update(0);
            __s_update = 0;
        }
    }
};

string mac_to_string(uint8_t * MAC);
uint32_t string_to_ip(string ip);
void string_to_mac(string smac, uint8_t * mac);

template <class T>
bool from_string(T& t, const std::string& s, std::ios_base& (*f)(std::ios_base&))
{
    istringstream iss(s);
    return !(iss >> f >> t).fail();
}

template <class T>
string to_string(T& t, std::ios_base& (*f)(std::ios_base&))
{
    ostringstream stream;
    stream << f << t;
    return stream.str();
}


#endif
