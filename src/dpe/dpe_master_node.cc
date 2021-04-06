#include "dpe/dpe_master_node.h"

#include <iostream>
#include <google/protobuf/text_format.h>

#include "dpe/dpe.h"
#include "dpe/dpe_internal.h"

namespace dpe {
static char buff[1024];
DPEMasterNode::DPEMasterNode(const std::string& my_ip, int port)
    : my_ip_(my_ip), port_(port), weakptr_factory_(this), last_save_time_(0) {
  GetModuleFileNameA(NULL, buff, 1024);
  std::string modulePath = buff;
  auto parentDir = base::FilePath(base::UTF8ToNative(modulePath)).DirName();
  module_dir_ = base::NativeToUTF8(parentDir.value());
}

DPEMasterNode::~DPEMasterNode() { Stop(); }

bool DPEMasterNode::Start() {
  zserver_ = new ZServer(this);
  if (!zserver_->Start(my_ip_, port_)) {
    zserver_ = NULL;
    LOG(WARNING) << "Cannot start master node.";
    LOG(WARNING) << "ip = " << my_ip_;
    LOG(WARNING) << "port = " << port_;
    return false;
  } else {
    LOG(INFO) << "ZServer starts at: " << zserver_->GetServerAddress();
  }

  class TaskAppenderImpl : public TaskAppender {
   public:
    TaskAppenderImpl(std::deque<int64>& task_queue_)
        : task_queue_(task_queue_) {}

    ~TaskAppenderImpl() {}

    void AddTask(int64 taskId) { task_queue_.push_back(taskId); }

   private:
    std::deque<int64>& task_queue_;
  };

  GetSolver()->InitMaster();
  TaskAppenderImpl appender(task_queue_);
  GetSolver()->GenerateTasks(&appender);
  LOG(INFO) << "Found " << task_queue_.size() << " tasks." << std::endl;

  if (GetFlags().read_state) {
    LoadState();
    if (task_queue_.empty() && task_running_queue_.empty()) {
      GetSolver()->Finish();
      WillExitDpe();
    }
  } else {
    SkipLoadState();
  }
  return true;
}

void DPEMasterNode::Stop() {
  if (zserver_) {
    zserver_->Stop();
    zserver_ = NULL;
  }
}

int DPEMasterNode::HandleRequest(const Request& req, Response& reply) {
  if (req.has_get_task()) {
    auto& get_task = req.get_task();
    const int max_task_count = std::max(get_task.max_task_count(), 1);
    int added = 0;
    auto* task = new GetTaskResponse();
    while (!task_queue_.empty() && added < max_task_count) {
      const int64 task_id = task_queue_.front();
      task_queue_.pop_front();
      task_running_queue_.insert(task_id);
      task->add_task_id(task_id);
      ++added;
    }
    reply.set_allocated_get_task(task);
    reply.set_error_code(0);
  } else if (req.has_finish_compute()) {
    auto& data = req.finish_compute();
    const int size = data.task_item_size();
    std::vector<int64> task_id;
    std::vector<int64> result;
    std::vector<int64> time_usage;
    for (int i = 0; i < size; ++i) {
      auto& item = data.task_item(i);
      if (task_running_queue_.count(item.task_id())) {
        all_result_.push_back({item.task_id(), item.result()});
        task_running_queue_.erase(item.task_id());

        task_id.push_back(item.task_id());
        result.push_back(item.result());
        time_usage.push_back(item.time_usage());
      }
    }

    if (size > 0) {
      GetSolver()->SetResult(task_id.size(), &task_id[0], &result[0],
                             &time_usage[0], data.total_time_usage());
      if (task_queue_.empty() && task_running_queue_.empty()) {
        SaveState(true);
        GetSolver()->Finish();
        WillExitDpe();
      } else {
        SaveState(false);
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
      base::FilePath filePath(base::UTF8ToNative(module_dir_ + "\\index.html"));
      if (!base::ReadFileToString(filePath, &data)) {
        return true;
      }
      rep->SetBody(data);
    } else if (req.path == "/jquery.min.js") {
      std::string data;
      base::FilePath filePath(
          base::UTF8ToNative(module_dir_ + "\\jquery.min.js"));
      if (!base::ReadFileToString(filePath, &data)) {
        return true;
      }
      rep->SetBody(data);
    } else if (req.path == "/Chart.bundle.js") {
      std::string data;
      base::FilePath filePath(
          base::UTF8ToNative(module_dir_ + "\\Chart.bundle.js"));
      if (!base::ReadFileToString(filePath, &data)) {
        return true;
      }
      rep->SetBody(data);
    }
  }
  return true;
}

void DPEMasterNode::SaveState(bool force_save) {
  int64 current_time = base::Time::Now().ToInternalValue();
  if (force_save || last_save_time_ == 0 ||
      (base::Time::FromInternalValue(current_time) -
       base::Time::FromInternalValue(last_save_time_))
              .InMinutes() > 3) {
    MasterState master_state;
    for (auto& iter : all_result_) {
      TaskItem* item = master_state.add_task_item();
      item->set_task_id(iter.first);
      item->set_result(iter.second);
    }

    google::protobuf::TextFormat::Printer printer;
    std::string data;
    printer.PrintToString(master_state, &data);

    base::FilePath file_path(
        base::UTF8ToNative(module_dir_ + "\\state.txtproto"));
    base::WriteFile(file_path, data.c_str(), data.size());

    LOG(INFO) << "Server stat saved, file size = " << data.size();
    last_save_time_ = current_time;
  }
}

void DPEMasterNode::LoadState() {
  base::FilePath file_path(
      base::UTF8ToNative(module_dir_ + "\\state.txtproto"));
  if (!base::PathExists(file_path)) {
    LOG(INFO) << "Cannot find master state file: " << file_path.AsUTF8Unsafe();
    return;
  }

  std::string data;
  if (!base::ReadFileToString(file_path, &data)) {
    LOG(ERROR) << "Canno read state file: " << file_path.AsUTF8Unsafe();
    return;
  }

  MasterState master_state;
  if (!google::protobuf::TextFormat::ParseFromString(data, &master_state)) {
    LOG(ERROR) << "Failed to parse state file: " << file_path.AsUTF8Unsafe();
    return;
  }

  std::map<int64, int64> task_to_result;
  const int size = master_state.task_item_size();
  for (int i = 0; i < size; ++i) {
    auto& item = master_state.task_item(i);
    if (item.has_result()) {
      task_to_result[item.task_id()] = item.result();
    }
  }

  LOG(INFO) << "Load " << all_result_.size() << " results from saved state.";

  std::vector<int64> task_id;
  std::vector<int64> result;
  std::vector<int64> time_usage;
  std::deque<int64> new_task_queue;
  for (auto& iter : task_queue_) {
    auto where = task_to_result.find(iter);
    if (where != task_to_result.end()) {
      all_result_.push_back({iter, where->second});
      task_id.push_back(iter);
      result.push_back(where->second);
      time_usage.push_back(0);
    } else {
      new_task_queue.push_back(iter);
    }
  }

  GetSolver()->SetResult(task_id.size(), &task_id[0], &result[0],
                         &time_usage[0], 0LL);

  task_queue_ = std::move(new_task_queue);
}

void DPEMasterNode::SkipLoadState() {
  base::FilePath file_path(
      base::UTF8ToNative(module_dir_ + "\\state.txtproto"));
  bool has_state = base::PathExists(file_path);
  LOG(INFO) << "Skip loading state.";
  LOG(INFO) << "State file exists: " << std::boolalpha << has_state;
}
}  // namespace dpe
