#ifndef __S_NAT_H
#define __S_NAT_H

#include <Wt/WGridLayout>
#include <Wt/WGroupBox>
#include <Wt/WInPlaceEdit>
#include <Wt/WIntValidator>
#include <Wt/WCssDecorationStyle>
#include <Wt/WColor>
#include <Wt/WBorder>
#include <Wt/WCheckBox>
#include <Wt/WComboBox>
#include <Wt/WVBoxLayout>
#include <Wt/WBoxLayout>

#include "FwdTable.h"
#include "ARPTables.h"


class NatTable : public WTable
{
    struct TableElt
    {
        uint32_t ip_in, ip_out, ip_dst;
        uint16_t port_in, port_out, port_dst;
        int ttl;

        TableElt(uint32_t i, uint32_t o, uint32_t d,
                 uint16_t pi, uint16_t po, uint16_t pd, int t) :
            ip_in(i), ip_out(o), ip_dst(d), port_in(pi), port_out(po), port_dst(pd), ttl(t)
        {}
    };

    static void __l(uint32_t i, uint32_t o, uint32_t d,
                    uint16_t pi, uint16_t po, uint16_t pd, int t, void * userdata)
    {
        ((vector<TableElt> *)userdata)->push_back(TableElt(i,o,d,pi,po,pd,t));
    }

public:
    NatTable()
    {
        setHeaderCount(1);
        Update();
    }

    void Update()
    {
        clear();

        elementAt(0, 0)->addWidget(new Wt::WText(" inbound "));
        elementAt(0, 1)->addWidget(new Wt::WText(" outbound "));
        elementAt(0, 2)->addWidget(new Wt::WText(" destination "));
        elementAt(0, 3)->addWidget(new Wt::WText(" ttl "));

        vector<TableElt> arr;

        nat_table_loop(TestServer::SR, __l, &arr);

        for(unsigned int i = 0; i < arr.size(); i++)
        {
            elementAt(i+1, 0)->addWidget(new Wt::WText(ip_to_string(arr[i].ip_in) + ":" +
                                                       to_string<uint16_t>(arr[i].port_in,dec)));
            elementAt(i+1, 1)->addWidget(new Wt::WText(ip_to_string(arr[i].ip_out) + ":" +
                                                       to_string<uint16_t>(arr[i].port_out,dec)));
            elementAt(i+1, 2)->addWidget(new Wt::WText(ip_to_string(arr[i].ip_dst) + ":" +
                                                       to_string<uint16_t>(arr[i].port_dst,dec)));
            elementAt(i+1, 3)->addWidget(new Wt::WText(to_string<int>(arr[i].ttl,dec)));
        }
    }
};

class S_NAT : public WContainerWidget
{
protected:
    NatTable * __nat;
    int __update;
public:
    S_NAT()
    {
        WPanel * p = new WPanel();
        p->setTitle("NAT Table");
        __nat = new NatTable();
        p->setCentralWidget(__nat);
        setStyleClass("FwdTable");
        addWidget(p);
    }

    void NeedUpdate()
    {
        __update = 1;
    }

    void Update()
    {
        if(__update)
        {
            __update = 0;
            __nat->Update();
        }
    }
};



#endif
