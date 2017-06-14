#include "dpe/dpe.h"

#include <iostream>

#include <windows.h>
#include <winsock2.h>
#include <Shlobj.h>
#pragma comment(lib, "ws2_32")

#include "dpe/dpe_master_node.h"
#include "dpe/dpe_worker_node.h"

namespace dpe
{
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

static inline void startNetwork()
{
  WSADATA wsaData;
  WORD sockVersion = MAKEWORD(2, 2);
  if (::WSAStartup(sockVersion, &wsaData) != 0)
  {
     std::cerr << "Cannot initialize wsa" << std::endl;
     exit(-1);
  }
}

static inline void stopNetwork()
{
  ::WSACleanup();
}

static inline std::string removePrefix(const std::string& s, char c)
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
Solver* solver;

static inline void run()
{
  LOG(INFO) << "running";
  LOG(INFO) << "type = " << type;
  LOG(INFO) << "myIP = " << myIP;
  LOG(INFO) << "serverIP = " << serverIP;

  if (type == "server")
  {
    dpeMasterNode = new DPEMasterNode(myIP, serverIP);
    dpeMasterNode->Start(port == 0 ? dpe::kServerPort : port);
  }
  else if (type == "worker")
  {
    dpeWorkerNode = new DPEWorkerNode(myIP, serverIP);
    dpeWorkerNode->Start(port == 0 ? dpe::kWorkerPort : port);
  }
  else
  {
    LOG(ERROR) << "Unknown type";
    base::will_quit_main_loop();
  }
}

static void exitDpeImpl()
{
  LOG(INFO) << "exitDpeImpl";
  dpeMasterNode = NULL;
  dpeWorkerNode = NULL;
  base::will_quit_main_loop();
}

void willExitDpe()
{
  LOG(INFO) << "willExitDpe";
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(exitDpeImpl));
}

Solver* getSolver()
{
  return solver;
}
}

using namespace dpe;

DPE_EXPORT void start_dpe(Solver* solver, int argc, char* argv[])
{
  ::solver = solver;
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
