#include "dpe_base/dpe_base.h"

#include <string>
#include <vector>
#include <iostream>

#include "process/process.h"

class ProcessMonitor : public process::ProcessHost {
 public:
  ProcessMonitor() {}
  ~ProcessMonitor() {
    const int size = static_cast<int>(processes.size());
    for (int i = 0; i < size; ++i) {
      processes[i]->Release();
    }
    std::vector<process::Process*>().swap(processes);
  }
  void start(const std::string& processName, int minId, int maxId,
             std::vector<std::string>& arguments) {
    base::FilePath imagePath(base::UTF8ToNative(processName));
    std::vector<NativeString> nativeArguments;
    for (auto& iter : arguments) {
      nativeArguments.push_back(base::UTF8ToNative(iter));
    }
    for (int i = minId; i <= maxId; ++i) {
      processes.push_back(new process::Process(this));
      auto& option = processes.back()->GetProcessOption();

      option.image_path_ = imagePath;
      option.argument_list_.push_back(base::UTF8ToNative("--id"));
      option.argument_list_.push_back(
          base::UTF8ToNative(base::StringPrintf("%d", i)));
      for (auto& iter : nativeArguments) {
        option.argument_list_.push_back(iter);
      }
      option.redirect_std_inout_ = true;
    }
    const int size = static_cast<int>(processes.size());
    for (int i = 0; i < size; ++i) {
      processes[i]->AddRef();
      processes[i]->Start();
      LOG(INFO) << "Process " << i << " starts";
    }
  }
  void OnStop(process::Process* p, process::ProcessContext* exit_code) {
    auto* np = new process::Process(this);
    np->GetProcessOption() = p->GetProcessOption();
    const int size = static_cast<int>(processes.size());
    for (int i = 0; i < size; ++i) {
      if (processes[i] == p) {
        processes[i]->Release();
        processes[i] = np;
        np->Start();
        LOG(INFO) << "Process " << i << " restarts";
        break;
      }
    }
  }
  void OnOutput(process::Process* p, bool is_std_out, const std::string& data) {

  }

 private:
  std::vector<process::Process*> processes;
};

std::string processName;
int minId = 0;
int maxId = 0;
std::vector<std::string> arguments;

ProcessMonitor monitor;
static void run() {
  LOG(INFO) << "runs";
  monitor.start(processName, minId, maxId, arguments);
}

static inline std::string parseCmd(const std::string& s, int& idx,
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

int main(int argc, char* argv[]) {
  int loggingLevel = 1;
  for (int i = 1; i < argc;) {
    int idx;
    std::string value;
    const std::string str = parseCmd(argv[i], idx, value);

    if (str == "") {
      for (int j = i + 1; j < argc; ++j) {
        arguments.push_back(argv[j]);
      }
      break;
    } else if (str == "l" || str == "log") {
      if (idx == -1) {
        loggingLevel = atoi(argv[i + 1]);
        i += 2;
      } else {
        loggingLevel = atoi(value.c_str());
        ++i;
      }
    } else if (str == "id") {
      std::string data;
      if (idx == -1) {
        data = argv[i + 1];
        i += 2;
      } else {
        data = value;
        ++i;
      }
      int l, r;
      if (sscanf(data.c_str(), "%d-%d", &l, &r) == 2) {
        minId = l;
        maxId = r;
      } else if (sscanf(data.c_str(), "%d", &l) == 1) {
        minId = maxId = l;
      } else {
        std::cerr << "Invalid id arguments" << std::endl;
        return -1;
      }
    } else {
      processName = argv[i];
      ++i;
    }
  }
  if (processName.empty()) {
    std::cerr << "Unspecified process name" << std::endl;
    return -1;
  }

  base::dpe_base_main(run, NULL, loggingLevel);

  return 0;
}