#ifndef _S_OSPF_H
#define _S_OSPF_H


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

#include "FwdTable.h"
#include "ARPTables.h"

class NTable : public WTable
{
    struct TableElt
    {
        string interface;
        uint32_t rid;
        int aid;
        uint32_t ip;
        uint32_t mask;
        int hello;
        int ttl;

        TableElt(string i, uint32_t r, int a, uint32_t _ip, uint32_t m, int h, int t) :
            interface(i), rid(r), aid(a), ip(_ip), mask(m), hello(h), ttl(t)
        {}
    };

    vector<TableElt> __arr;

    static void l_n(struct sr_vns_if * iface, struct neighbour * n, void * data)
    {
        vector<TableElt> * arr = (vector<TableElt> *)data;
        arr->push_back(TableElt(iface->name, n->router_id, n->aid, n->ip, n->nmask, n->helloint, n->ttl));
    }
    
public:
    NTable()
    {
        setHeaderCount(1);
        Update();
    }

    void Update()
    {
        clear();
        int j = 0;
        elementAt(0, j++)->addWidget(new Wt::WText(" interface "));
        elementAt(0, j++)->addWidget(new Wt::WText(" rid "));
        elementAt(0, j++)->addWidget(new Wt::WText(" aid "));
        elementAt(0, j++)->addWidget(new Wt::WText(" ip "));
        elementAt(0, j++)->addWidget(new Wt::WText(" mask "));
        elementAt(0, j++)->addWidget(new Wt::WText(" hello interval "));
        elementAt(0, j++)->addWidget(new Wt::WText(" ttl "));

        vector<TableElt> arr;

        interface_list_loop_through_neighbours(INTERFACE_LIST(TestServer::SR),l_n, &arr);

        for(unsigned int i = 0; i < arr.size(); i++)
        {
            j = 0;
            elementAt(i+1, j++)->addWidget(new Wt::WText(arr[i].interface));
            elementAt(i+1, j++)->addWidget(new Wt::WText(ip_to_string(arr[i].rid)));
            elementAt(i+1, j++)->addWidget(new Wt::WText(to_string<int>(arr[i].aid, dec)));
            elementAt(i+1, j++)->addWidget(new Wt::WText(ip_to_string(arr[i].ip)));
            elementAt(i+1, j++)->addWidget(new Wt::WText(ip_to_string(arr[i].mask)));
            elementAt(i+1, j++)->addWidget(new Wt::WText(to_string<int>(arr[i].hello, dec)));
            elementAt(i+1, j++)->addWidget(new Wt::WText(to_string<int>(arr[i].ttl, dec)));
        }
        __arr = arr;
    }

    void UpdateTtl()
    {
        vector<TableElt> arr;

        interface_list_loop_through_neighbours(INTERFACE_LIST(TestServer::SR),l_n, &arr);

        if(arr.size() == __arr.size())
        {
            for(unsigned int i = 0; i < arr.size(); i++)
            {
                reinterpret_cast<WText*>(elementAt(i+1, 6)->widget(0))->setText(to_string<int>(arr[i].ttl, dec));
            }
        }
    }
};

class S_OSPF : public WContainerWidget
{
    NTable * __ntable;
public:
    S_OSPF()
    {
        __ntable = new NTable();
        WPanel * pn = new WPanel();
        pn->setTitle("Neighbours");
        pn->setCollapsible(true);
        pn->setCentralWidget(__ntable);
        addWidget(pn);
        setStyleClass("FwdTable");
    }

    void Update()
    {
        __ntable->Update();
    }

    void UpdateTtl()
    {
        __ntable->UpdateTtl();
    }
};

#endif
