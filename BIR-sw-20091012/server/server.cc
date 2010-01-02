
#include "../router.h"
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



static TestServer * server = NULL;
struct sr_instance * TestServer::SR = NULL;
struct sr_mutex * TestServer::_updateMutex = NULL;
set<TestApp * > TestServer::_updateList;

static string ip_to_string(uint32_t ip)
{
    struct in_addr a;
    a.s_addr = ip;
    return inet_ntoa(a);
}

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

    void add_entry()
    {
        if(__wt_subnet->validate() != WValidator::Valid ||
           __wt_mask->validate() != WValidator::Valid ||
           __wt_next_hop->validate() != WValidator::Valid)
        {
            WMessageBox::show("Error", "Invalid Entry", Ok);
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

    static void l_interfaces(struct sr_vns_if * iface, void * data)
    {
        WComboBox * b = (WComboBox *)data;
        b->addItem(iface->name);
    }

    void Update()
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
            string ip1 ="(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)";
            string ip_pattern = ip1 + "\\." + ip1 + "\\." + ip1 + "\\." + ip1;
            __wt_subnet = new WLineEdit("");
            __wt_mask = new WLineEdit("");
            __wt_next_hop = new WLineEdit("");
            __wt_interface = new WComboBox();

            interface_list_loop_interfaces(TestServer::SR, l_interfaces, __wt_interface);

            __wt_subnet->setValidator(new WRegExpValidator(ip_pattern));
            __wt_mask->setValidator(new WRegExpValidator(ip_pattern));
            __wt_next_hop->setValidator(new WRegExpValidator(ip_pattern));
            
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
};


class FwdTables : public WContainerWidget
{
protected:
    FwdTable * __fwd_d, * __fwd_s;
    WPanel * __wpd, * __wps;
public:
    FwdTables()
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

        addWidget(__wpd);
        addWidget(new WBreak());
        addWidget(__wps);
    }

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

class TestApp : public WApplication
{
    FwdTables * __fwd;
public:
    TestApp(const WEnvironment & env) : WApplication(env)
    {
        enableUpdates();
        __fwd = new FwdTables();
        root()->addWidget(__fwd);
        TestServer::AddToUpdateList(this);

        styleSheet().addRule(new WCssTextRule("table, th, td","border: 1px solid black;"));
        styleSheet().addRule(new WCssTextRule("td,th","padding:0.4em;"));
    }

    ~TestApp()
    {
        TestServer::RemoveFromUpdateList(this);
    }

    void UpdateFwdTable(int dyn)
    {
        UpdateLock lock = getUpdateLock();
        __fwd->Update(dyn);
        triggerUpdate();
    }
};

WApplication * TestServer::createApplication(const WEnvironment & env)
{
    return new TestApp(env);
}

TestServer::TestServer(struct sr_instance * sr, int argc, char ** argv) : WServer("")
{
    SR = sr;
    _updateMutex = mutex_create();
    setServerConfiguration(argc, argv);
    addEntryPoint(Application, createApplication);
        
    if(start())
    {
        WServer::waitForShutdown();
        stop();
    }
}

TestServer::~TestServer()
{
    mutex_destroy(_updateMutex);
}

void TestServer::update_fwdtable(int dyn)
{
    mutex_lock(_updateMutex);
    for(set<TestApp *>::iterator ii=_updateList.begin(); ii!=_updateList.end(); ++ii)
    {
        (*ii)->UpdateFwdTable(dyn);
    }
    mutex_unlock(_updateMutex);
}

void server_update_fwdtable()
{
    server->update_fwdtable(1);
}

void server_update_fwdtable_s()
{
    server->update_fwdtable(0);
}


void TestServer::AddToUpdateList(TestApp * app)
{
    mutex_lock(_updateMutex);
    _updateList.insert(app);
    mutex_unlock(_updateMutex);
}

void TestServer::RemoveFromUpdateList(TestApp * app)
{
    mutex_lock(_updateMutex);
    _updateList.erase(app);
    mutex_unlock(_updateMutex);
}


extern "C" void stopserver()
{
    if(server)
    {
        server->stop();
        delete server;
        server = NULL;
    }
}

static void addstr(char ** arr, int & i, const char * str)
{
    arr[i] = (char*)malloc((strlen(str)+1) * sizeof(char));
    bzero(arr[i],strlen(str)+1);
    strcpy(arr[i],str);
    i++;
}

static void __runserver(void * data)
{
    struct sr_instance * sr = (struct sr_instance *)data;
    if(server == NULL)
    {
        char ** conf = (char **)malloc(9 * sizeof(char *));
        int i = 0;
        addstr(conf, i, "sr_server");
        addstr(conf, i, "--docroot");
        addstr(conf, i, "/usr/local/share/Wt/");
        addstr(conf, i, "--http-port");
        addstr(conf, i, router_get_http_port(sr));
        addstr(conf, i, "--http-address");
        addstr(conf, i, "0.0.0.0");
        addstr(conf, i, "--accesslog");
        addstr(conf, i, "/dev/null");

        try
        {
            server = new TestServer(sr, i, conf);
        }
        catch (WServer::Exception & e)
        {
            cout << e.what() << "\n";
        }
        catch (std::exception & e)
        {
            cout << "exception: " << e.what() << "\n";
        }
    }
}

extern "C" void runserver(struct sr_instance * sr)
{
    sys_thread_new(__runserver,sr);
}

