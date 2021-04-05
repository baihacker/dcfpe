#include "dpe/dpe_master_node.h"

#include <iostream>
#include "dpe/dpe.h"
#include "dpe/dpe_internal.h"

namespace dpe {
static char buff[1024];
DPEMasterNode::DPEMasterNode(const std::string& myIP, int port)
    : myIP(myIP), port(port), weakptr_factory_(this) {
  GetModuleFileNameA(NULL, buff, 1024);
  std::string modulePath = buff;
  auto parentDir = base::FilePath(base::UTF8ToNative(modulePath)).DirName();
  moduleDir = base::NativeToUTF8(parentDir.value());
}

DPEMasterNode::~DPEMasterNode() { Stop(); }

bool DPEMasterNode::Start() {
  zserver = new ZServer(this);
  if (!zserver->Start(myIP, port)) {
    zserver = NULL;
    LOG(WARNING) << "Cannot start master node.";
    LOG(WARNING) << "ip = " << myIP;
    LOG(WARNING) << "port = " << port;
    return false;
  } else {
    LOG(INFO) << "Zserver starts at: " << zserver->GetServerAddress();
  }

  class TaskAppenderImpl : public TaskAppender {
   public:
    TaskAppenderImpl(std::deque<int64>& taskQueue) : taskQueue(taskQueue) {}

    ~TaskAppenderImpl() {}

    void addTask(int64 taskId) { taskQueue.push_back(taskId); }

   private:
    std::deque<int64>& taskQueue;
  };
  TaskAppenderImpl appender(taskQueue);
  GetSolver()->initAsMaster(&appender);
  LOG(INFO) << taskQueue.size() << " tasks." << std::endl;
  return true;
}

void DPEMasterNode::Stop() {
  if (zserver) {
    zserver->Stop();
    zserver = NULL;
  }
}

int DPEMasterNode::HandleRequest(const Request& req, Response& reply) {
  if (req.has_get_task()) {
    if (!taskQueue.empty()) {
      const int64 task_id = taskQueue.front();
      taskQueue.pop_front();
      auto* task = new GetTaskResponse();
      task->set_task_id(task_id);
      taskRunningQueue.insert(task_id);
      reply.set_allocated_get_task(task);
    }
    reply.set_error_code(0);
  } else if (req.has_finish_compute()) {
    auto& data = req.finish_compute();
    const int64 task_id = data.task_id();
    auto& result = data.result();
    int64 int64_result = result.value_int64();
    if (taskRunningQueue.count(task_id)) {
      this->result.push_back({task_id, int64_result});
      GetSolver()->setResult(task_id, &int64_result, sizeof(int64),
                             data.time_usage());
      taskRunningQueue.erase(task_id);
      if (taskQueue.empty() && taskRunningQueue.empty()) {
        GetSolver()->finish();
        WillExitDpe();
      }
    }
    reply.set_error_code(0);
  }
  return 0;
}

bool DPEMasterNode::HandleRequest(const http::HttpRequest& req,
                                  http::HttpResponse* rep) {
  if (req.method == "GET") {
    if (req.path == "/status") {
    } else if (req.path == "/") {
      std::string data;
      base::FilePath filePath(base::UTF8ToNative(moduleDir + "\\index.html"));
      if (!base::ReadFileToString(filePath, &data)) {
        return true;
      }
      rep->SetBody(data);
    } else if (req.path == "/jquery.min.js") {
      std::string data;
      base::FilePath filePath(
          base::UTF8ToNative(moduleDir + "\\jquery.min.js"));
      if (!base::ReadFileToString(filePath, &data)) {
        return true;
      }
      rep->SetBody(data);
    } else if (req.path == "/Chart.bundle.js") {
      std::string data;
      base::FilePath filePath(
          base::UTF8ToNative(moduleDir + "\\Chart.bundle.js"));
      if (!base::ReadFileToString(filePath, &data)) {
        return true;
      }
      rep->SetBody(data);
    }
  }
  return true;
}
}  // namespace dpe
