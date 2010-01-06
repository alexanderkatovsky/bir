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
#include <Wt/WBoxLayout>

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



class TTable : public WTable
{
    struct Link
    {
        uint32_t rid,subnet,mask;
        Link(uint32_t r = 0, uint32_t s = 0, uint32_t m = 0) : rid(r), subnet(s), mask(m)
        {}
    };

    struct Node
    {
        uint32_t rid;
        int seq;
        vector<Link> links;

        Node(uint32_t r = 0, int s = 0) : rid(r), seq(s)
        {}
    };

    static void __get_links(uint32_t rid_node, int seq, uint32_t rid_link, struct ip_address ip, void * data)
    {
        map<uint32_t, Node> & m = *((map<uint32_t, Node> *)(data));
        if(m.find(rid_node) == m.end())
        {
            m[rid_node] = Node(rid_node, seq);
        }
        m[rid_node].links.push_back(Link(rid_link, ip.subnet, ip.mask));
    }
public:
    TTable()
    {
        setHeaderCount(1);
        Update();        
    }

    void Update()
    {
        clear();
        elementAt(0, 0)->addWidget(new Wt::WText(" node rid "));
        elementAt(0, 1)->addWidget(new Wt::WText(" LSN "));
        elementAt(0, 2)->addWidget(new Wt::WText(" link rid "));
        elementAt(0, 3)->addWidget(new Wt::WText(" subnet "));
        elementAt(0, 4)->addWidget(new Wt::WText(" mask "));

        map<uint32_t, Node> nodes;
        link_state_graph_loop_links(TestServer::SR, __get_links, &nodes);

        int row = 1;
        for(map<uint32_t, Node>::iterator ii = nodes.begin(); ii != nodes.end(); ++ii)
        {
            elementAt(row, 0)->addWidget(new Wt::WText(ip_to_string(ii->second.rid)));
            elementAt(row, 1)->addWidget(new Wt::WText(to_string<int>(ii->second.seq, dec)));
            row++;
            for(vector<Link>::iterator jj = ii->second.links.begin(); jj != ii->second.links.end(); ++jj)
            {
                elementAt(row, 2)->addWidget(new Wt::WText(ip_to_string(jj->rid)));
                elementAt(row, 3)->addWidget(new Wt::WText(ip_to_string(jj->subnet)));
                elementAt(row, 4)->addWidget(new Wt::WText(ip_to_string(jj->mask)));
                row++;
            }
        }
    }
};

class S_OSPF : public WContainerWidget
{
    NTable * __ntable;
    TTable * __ttable;

    int __update, __updateTtl;
public:
    S_OSPF()
    {
        __update = 0;
        __updateTtl = 0;
        setStyleClass("FwdTable");
        __ntable = new NTable();
        WPanel * pn = new WPanel();
        pn->setTitle("Neighbours");
        pn->setCollapsible(true);
        pn->setCentralWidget(__ntable);
        addWidget(pn);

        __ttable = new TTable();
        WPanel * pt = new WPanel();
        pt->setTitle("Topology");
        pt->setCollapsible(true);
        pt->setCentralWidget(__ttable);
        addWidget(new WBreak());
        addWidget(pt);
    }

    void Update()
    {
        if(__update)
        {
            __update = 0;
            __ntable->Update();
            __ttable->Update();
        }
        if(__updateTtl)
        {
            __updateTtl = 0;
            __ntable->UpdateTtl();
        }
    }

    void NeedUpdateTtl()
    {
        __updateTtl = 1;        
    }

    void NeedUpdate()
    {
        __update = 1;
    }
};

#endif
