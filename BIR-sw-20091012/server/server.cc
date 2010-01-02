
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

#include "FwdTable.h"

TestServer * TestServer::server = NULL;
struct sr_instance * TestServer::SR = NULL;
struct sr_mutex * TestServer::_updateMutex = NULL;
set<TestApp * > TestServer::_updateList;


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

        styleSheet().addRule(new WCssTextRule("table, th, td","border: 1px solid black; font-size:95%;"));
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
    TestServer::server->update_fwdtable(1);
}

void server_update_fwdtable_s()
{
    TestServer::server->update_fwdtable(0);
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
            TestServer::server = new TestServer(sr, i, conf);
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

