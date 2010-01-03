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

public:
    ARPTable(int isDynamic = 1) : _isDynamic(isDynamic)
    {
        setHeaderCount(1);
        Update();
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

    void Update();
};

class ARPTables : public WContainerWidget
{
protected:
    ARPTable * __arp_d, * __arp_s;
    WPanel * __wpd, * __wps;
public:
    ARPTables();

    void Update(int dyn)
    {
        if(dyn)
        {
            __arp_d->Update();
        }
        else
        {
            __arp_s->Update();
        }
    }
};

string mac_to_string(uint8_t * MAC);

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
