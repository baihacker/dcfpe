#include "dpe_base/dpe_base.h"

#include <string>
#include <vector>
#include <iostream>

#include <windows.h>
#include <winsock2.h>
#include <Shlobj.h>
#pragma comment(lib, "ws2_32")

#include "remote_shell/remote_server_node.h"
#include "remote_shell/local_server_node.h"
#include "remote_shell/message_sender.h"

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

int isRemote = 1;
std::string target;

scoped_refptr<rs::RemoteServerNode> remoteServerNode;
scoped_refptr<rs::LocalServerNode> localServerNode;

static void run()
{
  if (!target.empty()) {
    isRemote = 0;
  }

  if (isRemote) {
    remoteServerNode = new rs::RemoteServerNode(get_iface_address());
    remoteServerNode->start(rs::kRemoteServerPort);
  } else {
    localServerNode = new rs::LocalServerNode(get_iface_address());
    localServerNode->start(rs::kLocalServerPort);
    localServerNode->setTarget(target, rs::kRemoteServerPort);
    localServerNode->executeCommand();
  }
}

static inline std::string parseCmd(const std::string& s, int& idx, std::string& value)
{
  idx = -1;
  value = "";

  const int l = static_cast<int>(s.length());
  int i = 0;
  while (i < l && s[i] == '-') ++i;
  int j = i;
  while (j < l && s[j] != '=') ++j;
  if (j < l)
  {
    idx = j;
    value = s.substr(j+1);
  }
  return StringToLowerASCII(s.substr(i, j-i));
}

int main(int argc, char* argv[])
{
  int loggingLevel = 1;
  for (int i = 1; i < argc;)
  {
    int idx;
    std::string value;
    const std::string str = parseCmd(argv[i], idx, value);

    if (str == "l" || str == "log")
    {
      if (idx == -1)
      {
        loggingLevel = atoi(argv[i+1]);
        i += 2;
      }
      else {
        loggingLevel = atoi(value.c_str());
        ++i;
      }
    }
    else if (str == "t" || str == "target")
    {
      if (idx == -1)
      {
        target = argv[i+1];
        i += 2;
      }
      else
      {
        target = value;
        ++i;
      }
    }
  }

  startNetwork();

  base::dpe_base_main(run, NULL, loggingLevel);

  stopNetwork();
  return 0;
}