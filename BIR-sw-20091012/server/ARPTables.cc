#include <iomanip>
#include "ARPTables.h"
#include "FwdTable.h"


void string_to_mac(string smac, uint8_t * mac)
{
    for(int i = 0; i < 6; i++)
    {
        int r;
        from_string<int>(r,smac.substr(3*i, 2),hex);
        mac[i] = (uint8_t)r;
    }
}

string mac_to_string(uint8_t * MAC)
{
    string ret = "";
    for(int i = 0; i < 6; i++)
    {
         ostringstream stream;
        stream << hex << setw(2) << setfill('0') << setiosflags(ios::right) << (int)MAC[i];
        ret += (i == 0 ? "" : ":") + stream.str();
    }
    return ret;
}

void ARPTable::add_entry()
{
    if(__wt_mac->validate() != WValidator::Valid)
    {
        WMessageBox::show("Error", "Invalid IP Entry."
                          "A MAC Address must have the form xx:xx:xx:xx:xx:xx"
                          " where x is one of 0123456789abcdef", Ok);
    }
    else if(__wt_ip->validate() != WValidator::Valid)
    {
        WMessageBox::show("Error", "Invalid MAC Entry."
                          "An IP Address must have the form xx:xx:xx:xx"
                          " where x is one of 0123456789", Ok);
    }
    else if(__wt_ip->text().toUTF8() == "" ||
            __wt_mac->text().toUTF8() == "")
    {
        WMessageBox::show("Error", "Empty Entry", Ok);
    }
    else
    {        
        struct in_addr addr;
        uint8_t mac[ETHER_ADDR_LEN];
        inet_aton(__wt_ip->text().toUTF8().c_str(), &addr);
        uint32_t ip = addr.s_addr;
        string_to_mac(__wt_mac->text().toUTF8(), mac);
        arp_cache_add(TestServer::SR, ip, mac, 0);
    }
}

void ARPTable::Update()
{
    clear();
    elementAt(0, 0)->addWidget(new Wt::WText(" ip "));
    elementAt(0, 1)->addWidget(new Wt::WText(" MAC "));
    if(_isDynamic)
    {
        elementAt(0, 2)->addWidget(new Wt::WText(" ttl "));
    }
    vector<TableElt> arr;

    arp_cache_loop(TestServer::SR, l_arptable, &arr, _isDynamic);

    for(unsigned int i = 0; i < arr.size(); i++)
    {
        elementAt(i+1, 0)->addWidget(new Wt::WText(ip_to_string(arr[i].ip)));
        elementAt(i+1, 1)->addWidget(new Wt::WText(mac_to_string(arr[i].MAC)));
        if(_isDynamic)
        {
            elementAt(i+1, 2)->addWidget(new Wt::WText(to_string<int>(arr[i].ttl, dec)));
        }
        if(!_isDynamic)
        {
            WPushButton * wb = new WPushButton("delete");
            elementAt(i+1, 2)->addWidget(wb);
            wb->clicked().connect(SLOT(new PushButton(i,this), PushButton::delete_entry));
        }
    }
    if(!_isDynamic)
    {
        __wt_ip = new IPEdit();
        __wt_mac = new MACEdit();
            
        elementAt(arr.size()+1, 0)->addWidget(__wt_ip);
        elementAt(arr.size()+1, 1)->addWidget(__wt_mac);
            
        WPushButton * wb = new WPushButton("add");
        elementAt(arr.size()+1, 2)->addWidget(wb);
        wb->clicked().connect(SLOT(this, ARPTable::add_entry));
    }
    __arr = arr;
}

ARPTables::ARPTables()
{
    __arp_d = new ARPTable(1);
    __arp_s = new ARPTable(0);

    __wpd = new WPanel();
    __wps = new WPanel();

    __wpd->setTitle("Dynamic ARP Table");
    __wps->setTitle("Static ARP Table");
        
    __wpd->setCentralWidget(__arp_d);
    __wps->setCentralWidget(__arp_s);

    __wpd->setCollapsible(true);
    __wps->setCollapsible(true);

    setStyleClass("FwdTable");

    addWidget(__wpd);
    addWidget(new WBreak());
    addWidget(__wps);
}
