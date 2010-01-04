#ifndef S_ROUTER_H
#define S_ROUTER_H

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

class RIWInPlaceEdit : public WInPlaceEdit
{
public:
    RIWInPlaceEdit(string text) : WInPlaceEdit(text)
    {
/*        setStyleClass("RIWinPlaceEdit");
        WApplication::instance()->styleSheet().addRule(".RIWinPlaceEdit",
                                                       "background: #F6F9ED;"
                                                       "border: 1px black;");*/
        setToolTip("Click To Edit");
        setMinimumSize(WLength(8,WLength::FontEm),0);
        WCssDecorationStyle  wds;
        wds.setBackgroundColor(WColor("#F6F9ED"));
        wds.setBorder(WBorder(WBorder::Solid, WBorder::Thin, WColor("#C1CDCD")));
        setDecorationStyle(wds);
    }
};

#define cbCHANGED(name, widget, onerror, onsuccess)                     \
    void name(WString v)                                                \
    {                                                                   \
        string s = v.toUTF8();                                          \
        if(s == "")                                                     \
        {                                                               \
            WMessageBox::show("Error", "Empty Entry", Ok);Update();     \
        }                                                               \
        else if(widget->lineEdit()->validate() != WValidator::Valid)    \
        {                                                               \
            onerror;Update();                                           \
        }                                                               \
        else                                                            \
        {                                                               \
            onsuccess                                                   \
        }                                                               \
    }                                                                   \

class RouterInterface : public WGroupBox
{
    string __name;
    WInPlaceEdit * __ip;
    WInPlaceEdit * __mask;
    WInPlaceEdit * __mac;
    WInPlaceEdit * __aid;

    WCheckBox * __enabled;
    WCheckBox * __ospf;

    WComboBox * __nat;

    
    enum e_nat_type __get_nat_type()
    {
        switch(__nat->currentIndex())
        {
        case 0:
            return E_NAT_TYPE_NONE;
        case 1:
            return E_NAT_TYPE_INBOUND;
        default:
            return E_NAT_TYPE_OUTBOUND;
        }
    }

    cbCHANGED(__ip_changed, __ip, TestServer::ip_err();,
              interface_list_set_ip(TestServer::SR,(char*)__name.c_str(),
                                    string_to_ip(__ip->lineEdit()->text().toUTF8()),
                                    string_to_ip(__mask->text().toUTF8())););

    cbCHANGED(__mask_changed, __mask, TestServer::ip_err();,
              interface_list_set_ip(TestServer::SR,(char*)__name.c_str(),
                                    string_to_ip(__ip->text().toUTF8()),
                                    string_to_ip(__mask->lineEdit()->text().toUTF8())););
    
    cbCHANGED(__mac_changed, __mac, TestServer::mac_err();,
              uint8_t mac[ETHER_ADDR_LEN];
              string_to_mac(__mac->lineEdit()->text().toUTF8(), mac);
              interface_list_set_mac(TestServer::SR,(char*)__name.c_str(),mac););

    cbCHANGED(__aid_changed, __aid, WMessageBox::show("Error", "AID invalid", Ok);,
              uint32_t aid;
              from_string<uint32_t>(aid, s, dec);
              interface_list_set_params(TestServer::SR, (char *)__name.c_str(), aid,
                                        __ospf->checkState() == Checked,
                                        __get_nat_type(),
                                        __enabled->checkState() == Checked););

    void __param_changed()
    {
        string s_aid = __aid->text().toUTF8();
        uint32_t aid;
        from_string<uint32_t>(aid, s_aid, dec);
            
        interface_list_set_params(TestServer::SR, (char *)__name.c_str(), aid,
                                  __ospf->checkState() == Checked,
                                  __get_nat_type(),
                                  __enabled->checkState() == Checked);
    }
    
public:
    RouterInterface(struct sr_vns_if * iface) : WGroupBox(iface->name), __name(iface->name)
    {
        int i = 0;

        Wt::WContainerWidget *w = new Wt::WContainerWidget();

        __enabled = new WCheckBox("enabled");
        __enabled->setMargin(20,Left);
        addWidget(__enabled);
        __ospf = new WCheckBox("OSPF");
        __ospf->setMargin(20,Left);
        addWidget(__ospf);
        __nat = new WComboBox();
        __nat->setMargin(20,Left);
        __nat->addItem("NAT Disabled");
        __nat->addItem("NAT Inbound");
        __nat->addItem("NAT Outbound");
                
        addWidget(__nat);
        addWidget(new WBreak());
        WGridLayout * layout = new WGridLayout();
        
        __ip = new RIWInPlaceEdit("");
        IPEdit::Define(__ip->lineEdit());
        layout->addWidget(new WText("ip:"),i,0,AlignMiddle);
        layout->addWidget(__ip,i++,1,AlignMiddle);
        
        __mask = new RIWInPlaceEdit("");
        IPEdit::Define(__mask->lineEdit());
        layout->addWidget(new WText("mask:"),i,0,AlignMiddle);
        layout->addWidget(__mask,i++,1,AlignMiddle);
        
        __mac = new RIWInPlaceEdit("");
        MACEdit::Define(__mac->lineEdit());
        layout->addWidget(new WText("MAC:"), i,0,AlignMiddle);
        layout->addWidget(__mac,i++,1,AlignMiddle);

        __aid = new RIWInPlaceEdit("");
        __aid->lineEdit()->setValidator(new WIntValidator(0,0xffff));
        layout->addWidget(new WText("aid:"), i,0,AlignMiddle);
        layout->addWidget(__aid,i++,1,AlignMiddle);


//        layout->setVerticalSpacing(0);
        layout->setColumnStretch(0,0);
        layout->setColumnStretch(1,1);
        w->setLayout(layout,AlignTop);
        addWidget(w);
        addWidget(new WBreak());

        Update();

        __ip->valueChanged().connect(SLOT(this,RouterInterface::__ip_changed));
        __mask->valueChanged().connect(SLOT(this,RouterInterface::__mask_changed));
        __mac->valueChanged().connect(SLOT(this,RouterInterface::__mac_changed));
        __aid->valueChanged().connect(SLOT(this,RouterInterface::__aid_changed));
        __enabled->checked().connect(SLOT(this,RouterInterface::__param_changed));
        __enabled->unChecked().connect(SLOT(this,RouterInterface::__param_changed));
        __ospf->checked().connect(SLOT(this,RouterInterface::__param_changed));
        __ospf->unChecked().connect(SLOT(this,RouterInterface::__param_changed));
        __nat->activated().connect(SLOT(this,RouterInterface::__param_changed));
    }

    void Update()
    {
        struct sr_vns_if * iface = interface_list_get_interface_by_name(
            INTERFACE_LIST(TestServer::SR), (char *)__name.c_str());
        uint32_t aid;
        int ospf;
        enum e_nat_type nat_type;
        int up;
        interface_list_get_params(TestServer::SR,(char*) __name.c_str(), &aid,&ospf,&nat_type,&up);

        __nat->setCurrentIndex(nat_type ==  E_NAT_TYPE_NONE ? 0 :
                               (nat_type == E_NAT_TYPE_INBOUND ? 1 : 2));

        __enabled->setChecked(up ? Checked : Unchecked);
        __ospf->setChecked(ospf ? Checked : Unchecked);
        __aid->setText(to_string<uint32_t>(aid,dec));
        __ip->setText(ip_to_string(iface->ip));
        __mask->setText(ip_to_string(iface->mask));
        __mac->setText(mac_to_string(iface->addr));
    }
};

class RouterInfo : public WPanel
{
    RIWInPlaceEdit * __rid, * __aid;

    cbCHANGED(__rid_changed, __rid,
              TestServer::ip_err();,
              router_set_rid(TestServer::SR, string_to_ip(s)););

    cbCHANGED(__aid_changed, __aid,
              WMessageBox::show("Error", "AID invalid", Ok);,
              uint32_t aid;
              from_string<uint32_t>(aid, s, dec);
              router_set_aid(TestServer::SR, aid););

public:
    RouterInfo()
    {
        setTitle("Router Information");
        WContainerWidget * w = new WContainerWidget();
        WGridLayout * layout = new WGridLayout();
//        layout->setVerticalSpacing(0);
        setCentralWidget(w);
        setCollapsible(true);

        int i = 0;

        __rid = new RIWInPlaceEdit("");
        IPEdit::Define(__rid->lineEdit());
        layout->addWidget(new WText("rid:"),i,0,AlignMiddle);
        layout->addWidget(__rid,i++,1,AlignMiddle);
        
        __aid = new RIWInPlaceEdit("");
        __aid->lineEdit()->setValidator(new WIntValidator(0,0xffff));
        layout->addWidget(new WText("aid:"), i,0,AlignMiddle);
        layout->addWidget(__aid,i++,1,AlignMiddle);

        ostringstream stream;
        stream << __DATE__ << "," << __TIME__;
        layout->addWidget(new WText("Time:"), i,0,AlignMiddle);
        layout->addWidget(new WText(stream.str()),i++,1,AlignMiddle);

#ifdef _DEBUG_
        const char * debug_str = "Debug";
#else
        const char * debug_str = "Release";
#endif
#ifdef _CPUMODE_
        const char * cpu_str = "CPU";
#else
        const char * cpu_str = "VNS";
#endif    

        ostringstream stream2;
        stream2 << debug_str << "," << cpu_str;
        layout->addWidget(new WText("Type:"), i,0,AlignMiddle);
        layout->addWidget(new WText(stream2.str()),i++,1,AlignMiddle);
        layout->setColumnStretch(0,0);
        layout->setColumnStretch(1,1);
        w->setLayout(layout,AlignTop);
        Update();

        __aid->valueChanged().connect(SLOT(this,RouterInfo::__aid_changed));
        //__aid->saveButton()->clicked().connect(SLOT(this,RouterInfo::__aid_changed));
        __rid->valueChanged().connect(SLOT(this,RouterInfo::__rid_changed));
    }

    void Update()
    {
        uint32_t aid = router_get_aid(TestServer::SR);
        __rid->setText(ip_to_string(ROUTER(TestServer::SR)->rid));
        __aid->setText(to_string<uint32_t>(aid,dec));
    }
};

class S_Router : public WContainerWidget
{
    WGridLayout * __layout;
    vector<RouterInterface *> __interfaces;
    RouterInfo * __info;

    static void __add_interface(struct sr_vns_if * iface, void * userdata)
    {
        S_Router * r = (S_Router *)userdata;
        RouterInterface * ri = new RouterInterface(iface);
        int n = r->__interfaces.size();
        r->__interfaces.push_back(ri);
        r->__layout->addWidget(ri, n / 2, n % 2);
    }
    
public:
    S_Router()
    {
        Wt::WContainerWidget *w = new Wt::WContainerWidget();
        __layout = new WGridLayout();
        interface_list_loop_interfaces(TestServer::SR, __add_interface, this);
        __info = new RouterInfo();
        addWidget(__info);
        addWidget(new WBreak());
        addWidget(w);
        w->setLayout(__layout,AlignTop);
    }

    void Update()
    {
        for(vector<RouterInterface *>::iterator ii=__interfaces.begin(); ii!=__interfaces.end(); ++ii)
        {
            (*ii)->Update();
        }

        __info->Update();
    }
};


#endif
