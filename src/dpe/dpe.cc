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
  gethostname(hostname, 128);
  auto* temp = gethostbyname(hostname);
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

std::string GetExecutableDir() {
  char path[1024];
  GetModuleFileNameA(NULL, path, 1024);
  auto parentDir = base::FilePath(base::UTF8ToNative(path)).DirName();
  return base::NativeToUTF8(parentDir.value());
}

std::string GetDpeModuleDir() {
  HMODULE hm = NULL;
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         (LPCSTR)(void*)&GetDpeModuleDir, &hm) == 0) {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
    // Return or however you want to handle an error.
  }

  char path[MAX_PATH];
  if (GetModuleFileNameA(hm, path, sizeof(path)) == 0) {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
    // Return or however you want to handle an error.
  }

  auto parentDir = base::FilePath(base::UTF8ToNative(path)).DirName();
  return base::NativeToUTF8(parentDir.value());
}

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

static Flags flags;
const Flags& GetFlags() { return flags; }

Solver* solver;
Solver* GetSolver() { return solver; }

scoped_refptr<DPEMasterNode> master_node;
scoped_refptr<DPEWorkerNode> worker_node;
http::HttpServer http_server;

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
  LOG(INFO) << "executable dir = " << GetExecutableDir();
  LOG(INFO) << "dpe module dir = " << GetDpeModuleDir();
  LOG(INFO) << "type = " << flags.type;
  LOG(INFO) << "my_ip = " << flags.my_ip;
  LOG(INFO) << "server_ip = " << flags.server_ip;
  flags.server_port =
      flags.server_port == 0 ? dpe::kServerPort : flags.server_port;
  LOG(INFO) << "server_port = " << flags.server_port;
  LOG(INFO) << "logging_level = " << flags.logging_level;

  if (flags.type == "server") {
    LOG(INFO) << "read_state = " << std::boolalpha << flags.read_state;
    LOG(INFO) << "http_port = " << flags.http_port;
  }
  if (flags.type == "worker") {
    LOG(INFO) << "thread_number = " << flags.thread_number;
    LOG(INFO) << "batch_size = " << flags.batch_size;
    LOG(INFO) << "parallel_info = " << flags.parallel_info;
    if (flags.thread_number <= 0) {
      LOG(WARNING) << "thread_number should be greater than 0.";
      WillExitDpe();
    }
    if (flags.batch_size == 0) {
      LOG(WARNING) << "batch_size cannot be 0.";
      WillExitDpe();
    }
  }

  if (flags.type == "server") {
    master_node = new DPEMasterNode(flags.my_ip, flags.server_port);
    http_server.SetHandler(master_node);
    http_server.Start(flags.http_port);
    if (!master_node->Start()) {
      LOG(ERROR) << "Failed to start master node";
      WillExitDpe();
    }
  } else if (flags.type == "worker") {
    worker_node =
        new DPEWorkerNode(flags.my_ip, flags.server_ip, flags.server_port);
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

  flags.my_ip = GetInterfaceAddress();
  flags.server_ip = flags.my_ip;

  for (int i = 1; i < argc;) {
    int idx;
    std::string value;
    const std::string str = ParseCmd(argv[i], idx, value);
    if (str == "t" || str == "type") {
      if (idx == -1) {
        flags.type = argv[i + 1];
        i += 2;
      } else {
        flags.type = value;
        ++i;
      }
    } else if (str == "ip") {
      if (idx == -1) {
        flags.my_ip = argv[i + 1];
        i += 2;
      } else {
        flags.my_ip = value;
        ++i;
      }
    } else if (str == "server_ip") {
      if (idx == -1) {
        flags.server_ip = argv[i + 1];
        i += 2;
      } else {
        flags.server_ip = value;
        ++i;
      }
    } else if (str == "sp" || str == "server_port") {
      if (idx == -1) {
        flags.server_port = atoi(argv[i + 1]);
        i += 2;
      } else {
        flags.server_port = atoi(value.c_str());
        ++i;
      }
    } else if (str == "tn" || str == "thread_number") {
      if (idx == -1) {
        flags.thread_number = atoi(argv[i + 1]);
        i += 2;
      } else {
        flags.thread_number = atoi(value.c_str());
        ++i;
      }
    } else if (str == "bs" || str == "batch_size") {
      if (idx == -1) {
        flags.batch_size = atoi(argv[i + 1]);
        i += 2;
      } else {
        flags.batch_size = atoi(value.c_str());
        ++i;
      }
    } else if (str == "pi" || str == "parallel_info") {
      if (idx == -1) {
        flags.parallel_info = atoi(argv[i + 1]);
        i += 2;
      } else {
        flags.parallel_info = atoi(value.c_str());
        ++i;
      }
    } else if (str == "l" || str == "log") {
      if (idx == -1) {
        flags.logging_level = atoi(argv[i + 1]);
        i += 2;
      } else {
        flags.logging_level = atoi(value.c_str());
        ++i;
      }
    } else if (str == "rs" || str == "read_state") {
      std::string data;
      if (idx == -1) {
        data = argv[i + 1];
        i += 2;
      } else {
        data = value;
        ++i;
      }
      data = StringToLowerASCII(data);
      if (data == "true" || data == "1") {
        flags.read_state = true;
      } else if (data == "false" || data == "0") {
        flags.read_state = false;
      } else {
        flags.read_state = false;
      }
    } else if (str == "hp" || str == "http_port") {
      if (idx == -1) {
        flags.http_port = atoi(argv[i + 1]);
        i += 2;
      } else {
        flags.http_port = atoi(value.c_str());
        ++i;
      }
    } else {
      ++i;
    }
  }

  base::set_blocking_pool_thread_number(flags.thread_number);
  if (flags.thread_number <= 0) {
    fprintf(stderr, "The thread number should be greater than 0, but it is %d",
            flags.thread_number);
  } else {
    base::dpe_base_main(run, NULL, flags.logging_level);
  }

  StopNetwork();
}

static DpeStub __stub_impl = {&dpe::RunDpe};

DPE_EXPORT DpeStub* get_stub() { return &__stub_impl; }
}  // namespace dpe