
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
#include <Wt/WStackedWidget>
#include <Wt/WMenu>
#include <Wt/WHBoxLayout>
#include <Wt/WVBoxLayout>

#include "FwdTable.h"
#include "ARPTables.h"
#include "S_Router.h"

TestServer * TestServer::server = NULL;
struct sr_instance * TestServer::SR = NULL;
struct sr_mutex * TestServer::_updateMutex = NULL;
set<TestApp * > TestServer::_updateList;


class S_OSPF : public WContainerWidget
{
};

class S_NAT : public WContainerWidget
{
};

class TestApp : public WApplication
{
    S_Router * __router;
    FwdTables * __fwd;
    ARPTables * __arp;
    S_OSPF * __ospf;
    S_NAT * __nat;
    WStackedWidget * __contentsStack;
    
public:
    TestApp(const WEnvironment & env) : WApplication(env)
    {
        enableUpdates();

        __router = new S_Router();
        __fwd = new FwdTables();
        __arp = new ARPTables();
        __ospf = new S_OSPF();
        __nat = new S_NAT();

        __contentsStack = new WStackedWidget();
        // Show scrollbars when needed ...
        __contentsStack->setOverflow(WContainerWidget::OverflowAuto);
        // ... and work around a bug in IE (see setOverflow() documentation)
        __contentsStack->setPositionScheme(Relative);
        __contentsStack->setStyleClass("contents");

        WMenu *menu = new WMenu(__contentsStack, Vertical, 0);
        menu->setRenderAsList(true);
        menu->setStyleClass("menu");
/*        menu->setInternalPathEnabled();
          menu->setInternalBasePath("/");*/

        menu->addItem("Router Information", __router);
        menu->addItem("Forwarding Tables", __fwd);
        menu->addItem("ARP Tables", __arp);
        menu->addItem("OSPF", __ospf);
        menu->addItem("NAT Tables", __nat);

/*        setInternalPath(initialInternalPath);*/
        menu->select(0);

        /*
         * Add it all inside a layout
         */
        WHBoxLayout *horizLayout = new WHBoxLayout(root());
        WVBoxLayout *vertLayout = new WVBoxLayout;

        horizLayout->addWidget(menu, 0);
        horizLayout->addLayout(vertLayout, 1);
        vertLayout->addWidget(__contentsStack, 1);
        
        TestServer::AddToUpdateList(this);

        __style();    
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

    void UpdateARPTable(int dyn, int ttl)
    {
        UpdateLock lock = getUpdateLock();
        __arp->Update(dyn,ttl);
        triggerUpdate();
    }

    void UpdateIP()
    {
        UpdateLock lock = getUpdateLock();
        __router->Update();
        triggerUpdate();
    }
    
    void UpdateRouterTtl()
    {
        UpdateLock lock = getUpdateLock();
        __router->UpdateOSPF();
        triggerUpdate();
    }
private:
    void __style()
    {
        styleSheet().addRule(new WCssTextRule("ul.menu li","padding-top: 8px;"));
        styleSheet().addRule(new WCssTextRule("ul.menu a",
                                              "letter-spacing:1px;text-decoration:none;padding-bottom:5px;width:200px;"));
        styleSheet().addRule(new WCssTextRule(
                                 "ul.menu .itemselected, ul.menu .itemselected a, ul.menu .itemselected a",
                                 "color: black;"
                                 ""));
        styleSheet().addRule(new WCssTextRule("ul.menu",
                                              "margin:0px;"
                                              "padding-left:0px;"
                                              "list-style:none;"
                                              "width:200px;"
                                              "border-right: 3px solid #EEEEEE;"));

        styleSheet().addRule(new WCssTextRule("ul.menu .item, ul.menu a.item, ul.menu .item a ",
                                              "color: #7E7E7E;"));
        styleSheet().addRule(new WCssTextRule("ul.menu a:hover, ul.menu .item a:hover",
                                              "color: #70BD1A;"
                                              "text-decoration: none;"));
        styleSheet().addRule(new WCssTextRule("body, html",
                                              "font-family:arial,sans-serif;"
                                              "height: 100%;"
                                              "width: 100%;"
                                              "margin: 0px; padding: 0px; border: none;"
                                              "overflow: hidden;"));
        styleSheet().addRule(new WCssTextRule(".FwdTable td, .FwdTable th, .FwdTable > table",
                                              "border: 1px solid black;padding:0.3em;"));        
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
    TestServer::server = this;
        
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

void TestServer::update_ip()
{
    mutex_lock(_updateMutex);
    for(set<TestApp *>::iterator ii=_updateList.begin(); ii!=_updateList.end(); ++ii)
    {
        (*ii)->UpdateIP();
    }
    mutex_unlock(_updateMutex);
}

void TestServer::update_router_ttl()
{
    mutex_lock(_updateMutex);
    for(set<TestApp *>::iterator ii=_updateList.begin(); ii!=_updateList.end(); ++ii)
    {
        (*ii)->UpdateRouterTtl();
    }
    mutex_unlock(_updateMutex);
}

void TestServer::update_arptable(int dyn, int ttl)
{
    mutex_lock(_updateMutex);
    for(set<TestApp *>::iterator ii=_updateList.begin(); ii!=_updateList.end(); ++ii)
    {
        (*ii)->UpdateARPTable(dyn,ttl);
    }
    mutex_unlock(_updateMutex);
}

void server_update_router_ttl()
{
    if(TestServer::server) TestServer::server->update_router_ttl();
}

void server_update_fwdtable()
{
    if(TestServer::server) TestServer::server->update_fwdtable(1);
}

void server_update_ip()
{
    if(TestServer::server) TestServer::server->update_ip();
}

void server_update_fwdtable_s()
{
    if(TestServer::server) TestServer::server->update_fwdtable(0);
}

void server_update_arptable(int ttl)
{
    if(TestServer::server) TestServer::server->update_arptable(1,ttl);
}

void server_update_arptable_s()
{
    if(TestServer::server) TestServer::server->update_arptable(0,0);
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
    if(TestServer::server)
    {
        TestServer::server->stop();
        delete TestServer::server;
        TestServer::server = NULL;
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
    if(TestServer::server == NULL)
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
            new TestServer(sr, i, conf);
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

