#include "FwdTable.h"


string ip_to_string(uint32_t ip)
{
    struct in_addr a;
    a.s_addr = ip;
    return inet_ntoa(a);
}

void FwdTable::add_entry()
{
    if(__wt_subnet->validate() != WValidator::Valid ||
       __wt_mask->validate() != WValidator::Valid ||
       __wt_next_hop->validate() != WValidator::Valid)
    {
        WMessageBox::show("Error", "Invalid IP Entry."
                          "An IP Address must have the form xx:xx:xx:xx"
                          " where x is one of 0123456789", Ok);
    }
    else if(__wt_subnet->text().toUTF8() == "" ||
            __wt_mask->text().toUTF8() == "" ||
            __wt_next_hop->text().toUTF8() == "")
    {
        WMessageBox::show("Error", "Empty Entry", Ok);
    }
    else
    {        
        struct in_addr addr;
        inet_aton(__wt_subnet->text().toUTF8().c_str(), &addr);
        uint32_t subnet = addr.s_addr;
        inet_aton(__wt_mask->text().toUTF8().c_str(), &addr);
        uint32_t mask = addr.s_addr;
        inet_aton(__wt_next_hop->text().toUTF8().c_str(), &addr);
        uint32_t next_hop = addr.s_addr;
        char * interface = (char *)__wt_interface->currentText().toUTF8().c_str();
        struct ip_address ip = {subnet,mask};
        forwarding_table_add(TestServer::SR, &ip, next_hop, interface, 0, 0);
    }
}

void FwdTable::Update()
{
    clear();
    elementAt(0, 0)->addWidget(new Wt::WText(" subnet "));
    elementAt(0, 1)->addWidget(new Wt::WText(" mask "));
    elementAt(0, 2)->addWidget(new Wt::WText(" next hop "));
    elementAt(0, 3)->addWidget(new Wt::WText(" interface "));
    vector<FwdTableElt> arr;
    struct sr_instance * sr = TestServer::SR;
    forwarding_table_loop(FORWARDING_TABLE(sr), l_fwdtable, &arr, _isDynamic, 0);

    for(unsigned int i = 0; i < arr.size(); i++)
    {
        elementAt(i+1, 0)->addWidget(new Wt::WText(ip_to_string(arr[i].subnet)));
        elementAt(i+1, 1)->addWidget(new Wt::WText(ip_to_string(arr[i].mask)));
        elementAt(i+1, 2)->addWidget(new Wt::WText(ip_to_string(arr[i].next_hop)));
        elementAt(i+1, 3)->addWidget(new Wt::WText(arr[i].interface));
        if(!_isDynamic)
        {
            WPushButton * wb = new WPushButton("delete");
            elementAt(i+1, 4)->addWidget(wb);
            wb->clicked().connect(SLOT(new PushButton(i,this), PushButton::delete_entry));
        }
    }
    if(!_isDynamic)
    {
        __wt_subnet = new IPEdit();
        __wt_mask = new IPEdit();
        __wt_next_hop = new IPEdit();
        __wt_interface = new WComboBox();


        interface_list_loop_interfaces(TestServer::SR, l_interfaces, __wt_interface);

        elementAt(arr.size()+1, 0)->addWidget(__wt_subnet);
        elementAt(arr.size()+1, 1)->addWidget(__wt_mask);
        elementAt(arr.size()+1, 2)->addWidget(__wt_next_hop);
        elementAt(arr.size()+1, 3)->addWidget(__wt_interface);
            
        WPushButton * wb = new WPushButton("add");
        elementAt(arr.size()+1, 4)->addWidget(wb);
        wb->clicked().connect(SLOT(this, FwdTable::add_entry));
    }
    __arr = arr;
}

FwdTables::FwdTables()
{
    __fwd_d = new FwdTable(1);
    __fwd_s = new FwdTable(0);

    __wpd = new WPanel();
    __wps = new WPanel();

    __wpd->setTitle("Dynamic Forwarding Table");
    __wps->setTitle("Static Forwarding Table");
        
    __wpd->setCentralWidget(__fwd_d);
    __wps->setCentralWidget(__fwd_s);

    __wpd->setCollapsible(true);
    __wps->setCollapsible(true);

    setStyleClass("FwdTable");

    addWidget(__wpd);
    addWidget(new WBreak());
    addWidget(__wps);
}
