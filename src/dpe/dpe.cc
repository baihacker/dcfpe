#include "dpe/dpe.h"

#include <iostream>

#include <windows.h>
#include <winsock2.h>
#include <Shlobj.h>
#pragma comment(lib, "ws2_32")

#include "dpe/dpe_internal.h"
#include "dpe/dpe_master_node.h"
#include "dpe/dpe_worker_node.h"
#include "dpe/http_server.h"

namespace dpe {
static inline std::string GetInterfaceAddress() {
  char hostname[128];
  char local_host[128][32] = {{0}};
  struct hostent* temp;
  gethostname(hostname, 128);
  temp = gethostbyname(hostname);
  for (int i = 0; temp->h_addr_list[i] != NULL && i < 1; ++i) {
    strcpy(local_host[i], inet_ntoa(*(struct in_addr*)temp->h_addr_list[i]));
  }
  return local_host[0];
}

static inline void StartNetwork() {
  WSADATA wsaData;
  WORD sockVersion = MAKEWORD(2, 2);
  if (::WSAStartup(sockVersion, &wsaData) != 0) {
    std::cerr << "Cannot initialize wsa" << std::endl;
    exit(-1);
  }
}

static inline void StopNetwork() { ::WSACleanup(); }

static inline std::string ParseCmd(const std::string& s, int& idx,
                                   std::string& value) {
  idx = -1;
  value = "";

  const int l = static_cast<int>(s.length());
  int i = 0;
  while (i < l && s[i] == '-') ++i;
  int j = i;
  while (j < l && s[j] != '=') ++j;
  if (j < l) {
    idx = j;
    value = s.substr(j + 1);
  }
  return StringToLowerASCII(s.substr(i, j - i));
}

Solver* solver;
Solver* GetSolver() { return solver; }

scoped_refptr<DPEMasterNode> master_node;
scoped_refptr<DPEWorkerNode> worker_node;
http::HttpServer http_server;
int http_port = 80;
std::string type = "server";
std::string my_ip;
std::string server_ip;
int logging_level = 0;
int server_port = 0;
// The max task count in GetTaskRequest.
// If the value is zero, use thread_number * 3.
// If max task count is zero, the server use 1.
int batch_size = 0;
// Currently, thread number is forwarded to worker ans an argument.
// This worker is not responsible for executing the tasks in parallel.
int thread_number = 1;

int GetBatchSize() { return batch_size; }
int GetThreadNumber() { return thread_number; }

static void ExitDpeImpl() {
  LOG(INFO) << "ExitDpeImpl";
  if (master_node) {
    master_node->Stop();
  }
  master_node = NULL;
  http_server.Stop();
  if (worker_node) {
    worker_node->Stop();
  }
  worker_node = NULL;
  base::will_quit_main_loop();
}

void WillExitDpe() {
  LOG(INFO) << "WillExitDpe";
  http_server.SetHandler(NULL);
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
                             base::Bind(ExitDpeImpl));
}

static inline void run() {
  LOG(INFO) << "type = " << type;
  LOG(INFO) << "my_ip = " << my_ip;
  LOG(INFO) << "server_ip = " << server_ip;
  server_port = server_port == 0 ? dpe::kServerPort : server_port;
  LOG(INFO) << "server_port = " << server_port;
  LOG(INFO) << "logging_level = " << logging_level;

  if (type == "server") {
    LOG(INFO) << "http_port = " << http_port;
  }
  if (type == "worker") {
    LOG(INFO) << "batch_size = " << batch_size;
    LOG(INFO) << "thread_number = " << thread_number;
  }

  if (type == "server") {
    master_node = new DPEMasterNode(my_ip, server_port);
    http_server.SetHandler(master_node);
    http_server.Start(http_port);
    if (!master_node->Start()) {
      LOG(ERROR) << "Failed to start master node";
      WillExitDpe();
    }
  } else if (type == "worker") {
    worker_node = new DPEWorkerNode(server_ip, server_port);
    if (!worker_node->Start()) {
      LOG(ERROR) << "Failed to start worker node";
      WillExitDpe();
    }
  } else {
    LOG(ERROR) << "Unknown type";
    WillExitDpe();
  }
}

void RunDpe(Solver* solver, int argc, char* argv[]) {
  std::cout << "DCFPE (version 2.0.0.0)" << std::endl;
  std::cout << "Author: baihacker" << std::endl;
  std::cout << "HomePage: https://github.com/baihacker/dcfpe" << std::endl;
  std::cout << std::endl;

  dpe::solver = solver;
  StartNetwork();

  type = "server";
  my_ip = GetInterfaceAddress();
  server_ip = my_ip;

  for (int i = 1; i < argc;) {
    int idx;
    std::string value;
    const std::string str = ParseCmd(argv[i], idx, value);
    if (str == "t" || str == "type") {
      if (idx == -1) {
        type = argv[i + 1];
        i += 2;
      } else {
        type = value;
        ++i;
      }
    } else if (str == "ip") {
      if (idx == -1) {
        my_ip = argv[i + 1];
        i += 2;
      } else {
        my_ip = value;
        ++i;
      }
    } else if (str == "server_ip") {
      if (idx == -1) {
        server_ip = argv[i + 1];
        i += 2;
      } else {
        server_ip = value;
        ++i;
      }
    } else if (str == "sp" || str == "server_port") {
      if (idx == -1) {
        server_port = atoi(argv[i + 1]);
        i += 2;
      } else {
        server_port = atoi(value.c_str());
        ++i;
      }
    } else if (str == "tn" || str == "thread_number") {
      if (idx == -1) {
        thread_number = atoi(argv[i + 1]);
        i += 2;
      } else {
        thread_number = atoi(value.c_str());
        ++i;
      }
    } else if (str == "bs" || str == "batch_size") {
      if (idx == -1) {
        batch_size = atoi(argv[i + 1]);
        i += 2;
      } else {
        batch_size = atoi(value.c_str());
        ++i;
      }
    } else if (str == "l" || str == "log") {
      if (idx == -1) {
        logging_level = atoi(argv[i + 1]);
        i += 2;
      } else {
        logging_level = atoi(value.c_str());
        ++i;
      }
    } else if (str == "hp" || str == "http_port") {
      if (idx == -1) {
        http_port = atoi(argv[i + 1]);
        i += 2;
      } else {
        http_port = atoi(value.c_str());
        ++i;
      }
    } else {
      ++i;
    }
  }

  base::dpe_base_main(run, NULL, logging_level);

  StopNetwork();
}

static DpeStub __stub_impl = {&dpe::RunDpe};

DPE_EXPORT DpeStub* get_stub() { return &__stub_impl; }
}  // namespace dpe