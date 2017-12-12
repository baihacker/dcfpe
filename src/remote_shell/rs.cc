#include "dpe_base/dpe_base.h"

#include <string>
#include <vector>
#include <iostream>

#include <windows.h>
#include <winsock2.h>
#include <Shlobj.h>
#pragma comment(lib, "ws2_32")

#include "remote_shell/listener_node.h"
#include "remote_shell/remote_server_node.h"
#include "remote_shell/local_shell.h"
#include "remote_shell/script_engine.h"

std::string get_iface_address() {
  char hostname[128];
  char localHost[128][32] = {{0}};
  struct hostent* temp;
  gethostname(hostname, 128);
  temp = gethostbyname(hostname);
  for(int i = 0; temp->h_addr_list[i] != NULL && i < 1; ++i) {
    strcpy(localHost[i], inet_ntoa(*(struct in_addr *)temp->h_addr_list[i]));
  }
  return localHost[0];
}

// Flags and the corresponding value.
int isListener = 0;
int sid = 0;
std::string host;
std::string target;
std::string scriptFile;
std::string action;

// Server or shell.
scoped_refptr<rs::ListenerNode> listenerNode;
scoped_refptr<rs::RemoteServerNode> remoteServerNode;
rs::LocalShell* localShell;
rs::ScriptEngine* scriptEngine;

static void run() {
  if (isListener) {
    listenerNode = new rs::ListenerNode(get_iface_address());
    if (!listenerNode->listen()) {
      LOG(ERROR) << "Cannot start lisener node.";
      listenerNode = NULL;
      base::will_quit_main_loop();
    }
  } else if (!host.empty()) {
    remoteServerNode = new rs::RemoteServerNode(get_iface_address());
    if (!remoteServerNode->start()) {
      LOG(ERROR) << "Cannot start remote server node.";
      remoteServerNode = NULL;
      base::will_quit_main_loop();
    } else {
      remoteServerNode->connectToHost(host, sid);
    }
  } else if (!target.empty()) {
    localShell = new rs::LocalShell();
    localShell->start(target);
  } else if (!scriptFile.empty()) {
    scriptEngine = new rs::ScriptEngine();
    scriptEngine->runScript(scriptFile, action);
  } else {
    LOG(ERROR) << "Cannot parse the arguments.";
    base::will_quit_main_loop();
  }
}

static inline std::string parseCmd(const std::string& s, int& idx, std::string& value) {
  idx = -1;
  value = "";

  const int l = static_cast<int>(s.length());
  int i = 0;
  while (i < l && s[i] == '-') ++i;
  int j = i;
  while (j < l && s[j] != '=') ++j;
  if (j < l) {
    idx = j;
    value = s.substr(j+1);
  }
  return StringToLowerASCII(s.substr(i, j-i));
}

static inline void startNetwork() {
  WSADATA wsaData;
  WORD sockVersion = MAKEWORD(2, 2);
  if (::WSAStartup(sockVersion, &wsaData) != 0) {
     std::cerr << "Cannot initialize wsa" << std::endl;
     exit(-1);
  }
}

static inline void stopNetwork() {
  ::WSACleanup();
}

int main(int argc, char* argv[]) {
  int loggingLevel = 1;
  for (int i = 1; i < argc;) {
    int idx;
    std::string value;
    const std::string str = parseCmd(argv[i], idx, value);

    if (str == "l" || str == "log") {
      if (idx == -1) {
        loggingLevel = atoi(argv[i+1]);
        i += 2;
      }
      else {
        loggingLevel = atoi(value.c_str());
        ++i;
      }
    } else if (str == "b" || str == "bind") {
      isListener = 1;
      ++i;
    } else if (str == "h" || str == "host") {
      if (idx == -1) {
        host = argv[i+1];
        i += 2;
      }
      else {
        host = value;
        ++i;
      }
    } else if (str == "s" || str == "sid") {
      if (idx == -1) {
        sid = atoi(argv[i+1]);
        i += 2;
      }
      else {
        sid = atoi(value.c_str());
        ++i;
      }
    } else if (str == "t" || str == "target") {
      if (idx == -1) {
        target = argv[i+1];
        i += 2;
      }
      else {
        target = value;
        ++i;
      }
    } else if (str == "f" || str == "file") {
      if (idx == -1) {
        scriptFile = argv[i+1];
        i += 2;
      }
      else {
        scriptFile = value;
        ++i;
      }
    } else if (str == "a" || str == "action") {
      if (idx == -1) {
        action = argv[i+1];
        i += 2;
      }
      else {
        action = value;
        ++i;
      }
    } else {
      fprintf(stderr, "Unknown arguments.\n");
      exit(-1);
    }
  }

  startNetwork();

  base::dpe_base_main(run, NULL, loggingLevel);

  stopNetwork();

  return 0;
}