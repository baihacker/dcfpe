#include "dpe/dpe.h"
#include "dpe_base/dpe_base.h"
#include "dpe/zserver.h"
#include "dpe/remote_node_impl.h"
#include "dpe/dpe_master_node.h"
#include "dpe/dpe_worker_node.h"

#include <iostream>

#include <windows.h>
#include <winsock2.h>
#include <Shlobj.h>
#pragma comment(lib, "ws2_32")

namespace dpe
{
class RepeatedActionrapperImpl : public RepeatedActionWrapper,
  public base::RepeatedActionHost
{
public:
  RepeatedActionrapperImpl() : refCount(0), weakptr_factory_(this) {}
  void addRef()
  {
    ++refCount;
  }

  void release()
  {
    if (--refCount == 0)
    {
      delete this;
    }
  }

  void start(std::function<void ()> action, int delay, int period)
  {
    repeatedAction = new base::RepeatedAction(this);
    repeatedAction->Start(
      base::Bind(&RepeatedActionrapperImpl::doAction, action),
      base::TimeDelta::FromSeconds(delay),
      base::TimeDelta::FromSeconds(period),
      -1);
  }

  static void doAction(std::function<void ()> action)
  {
    action();
  }
  void OnRepeatedActionFinish(base::RepeatedAction* ra)
  {
    
  }
  void stop()
  {
    if (repeatedAction)
    {
      repeatedAction->Stop();
      repeatedAction = NULL;
    }
  }
private:
  int refCount;
  scoped_refptr<base::RepeatedAction> repeatedAction;
  base::WeakPtrFactory<RepeatedActionrapperImpl> weakptr_factory_;
};

RepeatedActionWrapper* createRepeatedActionWrapper()
{
  return new RepeatedActionrapperImpl();
}

static inline std::string get_iface_address()
{
  char hostname[128];
  char localHost[128][32]={{0}};
  struct hostent* temp;
  gethostname(hostname, 128);
  temp = gethostbyname( hostname );
  for(int i=0 ; temp->h_addr_list[i] != NULL && i < 1; ++i)
  {
    strcpy(localHost[i], inet_ntoa(*(struct in_addr *)temp->h_addr_list[i]));
  }
  return localHost[0];
}

void startNetwork()
{
  WSADATA wsaData;
  WORD sockVersion = MAKEWORD(2, 2);
  if (::WSAStartup(sockVersion, &wsaData) != 0)
  {
     std::cerr << "Cannot initialize wsa" << std::endl;
     exit(-1);
  }
}

void stopNetwork()
{
  ::WSACleanup();
}

std::string removePrefix(const std::string& s, char c)
{
  const int l = static_cast<int>(s.length());
  int i = 0;
  while (i < l && s[i] == c) ++i;
  return s.substr(i);
}

scoped_refptr<DPEMasterNode> dpeMasterNode;
scoped_refptr<DPEWorkerNode> dpeWorkerNode;
std::string type = "server";
std::string myIP;
std::string serverIP;
int port = 0;

void run()
{
  LOG(INFO) << "running";
  LOG(INFO) << "type = " << type;
  LOG(INFO) << "myIP = " << myIP;
  LOG(INFO) << "serverIP = " << serverIP;

  if (type == "server")
  {
    dpeMasterNode = new DPEMasterNode(myIP, serverIP);
    dpeMasterNode->Start(port == 0 ? kServerPort : port);
  }
  else if (type == "worker")
  {
    dpeWorkerNode = new DPEWorkerNode(myIP, serverIP);
    dpeWorkerNode->Start(port == 0 ? kWorkerPort : port);
  }
  else
  {
    LOG(ERROR) << "Unknown type";
    base::quit_main_loop();
  }
}

void start_dpe(int argc, char* argv[])
{
  startNetwork();

  type = "server";
  myIP = get_iface_address();
  serverIP = myIP;

  int loggingLevel = 1;
  for (int i = 1; i < argc;)
  {
    const std::string str = removePrefix(argv[i], '-');

    if (str == "t")
    {
      type = argv[i+1];
      i += 2;
    }
    else if (str == "s")
    {
      serverIP = argv[i+1];
      i += 2;
    }
    else if (str == "l") {
      loggingLevel = atoi(argv[i+1]);
      i += 2;
    }
    else if (str == "p") {
      port = atoi(argv[i+1]);
      i += 2;
    }
    else
    {
      ++i;
    }
  }

  base::dpe_base_main(run, NULL, loggingLevel);
  
  stopNetwork();
}
}