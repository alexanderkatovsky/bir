#ifndef __FWD_TABLE_H
#define __FWD_TABLE_H

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


class FwdTable : public WTable
{
    int _isDynamic;
    
    struct FwdTableElt
    {
        uint32_t subnet, mask, next_hop;
        string interface;
        FwdTableElt(uint32_t s, uint32_t m, uint32_t n, string i) :
            subnet(s), mask(m), next_hop(n), interface(i)
        {}
    };

    class PushButton : public WPushButton
    {
        int _i;
        FwdTable * _fwd;
    public:
        PushButton(int i, FwdTable * fwd) : _i(i), _fwd(fwd){}
        void delete_entry()
        {
            _fwd->delete_row(_i);
        }
    };

    vector<FwdTableElt> __arr;
    WLineEdit * __wt_subnet;
    WLineEdit * __wt_mask;
    WLineEdit * __wt_next_hop;
    WComboBox * __wt_interface;

public:
    FwdTable(int isDynamic = 1) : _isDynamic(isDynamic)
    {
        setHeaderCount(1);
        Update();
    }

    static void l_fwdtable(uint32_t subnet, uint32_t mask, uint32_t next_hop,
                           char * interface, void * userdata, int * finished)
    {
        ((vector<FwdTableElt> *)userdata)->push_back(FwdTableElt(subnet,mask,next_hop,interface));
    }

    void delete_row(int i)
    {
        struct ip_address ip = {__arr[i].subnet, __arr[i].mask};
        forwarding_table_remove(TestServer::SR, &ip, 0, 0);
    }

    void add_entry();

    static void l_interfaces(struct sr_vns_if * iface, void * data)
    {
        WComboBox * b = (WComboBox *)data;
        b->addItem(iface->name);
    }

    void Update();
};

class FwdTables : public WContainerWidget
{
protected:
    FwdTable * __fwd_d, * __fwd_s;
    WPanel * __wpd, * __wps;
public:
    FwdTables();

    void Update(int dyn)
    {
        if(dyn)
        {
            __fwd_d->Update();
        }
        else
        {
            __fwd_s->Update();
        }
    }
};

string ip_to_string(uint32_t ip);

class IPEdit : public WLineEdit
{
public:
    IPEdit() : WLineEdit("")
    {
        string ip1 ="(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)";
        string ip_pattern = ip1 + "\\." + ip1 + "\\." + ip1 + "\\." + ip1;
        setMinimumSize(WLength(10,WLength::FontEm), WLength(0));
        setValidator(new WRegExpValidator(ip_pattern));
    }
};

class MACEdit : public WLineEdit
{
public:
    MACEdit() : WLineEdit("")
    {
        string mac1 = "[0-9a-fA-F][0-9a-fA-F]";
        string mac_pattern = mac1 + ":" + mac1 + ":" + mac1 + ":" + mac1 + ":" + mac1 + ":" + mac1;
        setMinimumSize(WLength(15,WLength::FontEm), WLength(0));
        setValidator(new WRegExpValidator(mac_pattern));
    }
};



#endif
