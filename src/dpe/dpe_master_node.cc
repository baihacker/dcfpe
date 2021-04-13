#include "dpe/dpe_master_node.h"

#include <iostream>
#include <google/protobuf/text_format.h>

#include "dpe/dpe.h"
#include "dpe/dpe_internal.h"

namespace dpe {

DPEMasterNode::DPEMasterNode(const std::string& my_ip, int port)
    : my_ip_(my_ip), port_(port), weakptr_factory_(this), last_save_time_(0) {
  executable_dir_ = GetExecutableDir();
  dpe_module_dir_ = GetDpeModuleDir();
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
    LOG(INFO) << "ZServer starts at: " << zserver_->GetServerAddress()
              << std::endl;
  }

  GetSolver()->InitMaster();
  int task_count = GetSolver()->GetTaskCount();
  task_queue_.resize(task_count);
  GetSolver()->GenerateTasks(&task_queue_[0]);
  LOG(INFO) << "Found " << task_queue_.size() << " tasks.";

  for (auto& iter : task_queue_) {
    TaskItem item;
    item.set_task_id(iter);
    item.set_status(TaskItem::TaskStatus::TaskItem_TaskStatus_PENDING);
    task_map_[iter].CopyFrom(item);
    task_pending_queue_.push_back(iter);
  }
  if (GetFlags().read_state) {
    LoadState();
    if (task_pending_queue_.empty() && task_running_queue_.empty()) {
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
  const auto current_time = base::Time::Now().ToInternalValue();
  auto& worker = GetWorker(req.worker_id());
  worker.set_request_count(worker.request_count() + 1);
  worker.set_latency_sum(worker.latency_sum() + current_time -
                         req.request_timestamp());
  worker.set_updated_time(current_time);

  if (req.has_get_task()) {
    auto& get_task = req.get_task();
    const int max_task_count = std::max(get_task.max_task_count(), 1);
    int added = 0;
    auto* task = new GetTaskResponse();

    while (!task_pending_queue_.empty() && added < max_task_count) {
      const int64 task_id = task_pending_queue_.front();
      task_pending_queue_.pop_front();
      task_running_queue_.insert(task_id);
      worker.add_running_task(task_id);
      task_map_[task_id].set_status(
          TaskItem::TaskStatus::TaskItem_TaskStatus_RUNNING);
      task->add_task_id(task_id);
      ++added;
    }

    reply.set_allocated_get_task(task);
    reply.set_error_code(0);
  } else if (req.has_finish_compute()) {
    auto& data = req.finish_compute();
    const int size = data.task_item_size();

    std::set<int64> removed_task_id;
    std::vector<int64> task_id;
    std::vector<int64> result;
    std::vector<int64> time_usage;
    for (int i = 0; i < size; ++i) {
      auto& item = data.task_item(i);
      removed_task_id.insert(item.task_id());
      if (task_running_queue_.count(item.task_id())) {
        auto& my_item = task_map_[item.task_id()];
        my_item.set_result(item.result());
        my_item.set_time_usage(item.time_usage());
        my_item.set_status(TaskItem::TaskStatus::TaskItem_TaskStatus_DONE);

        task_running_queue_.erase(item.task_id());

        task_id.push_back(item.task_id());
        result.push_back(item.result());
        time_usage.push_back(item.time_usage());
      }
    }

    std::vector<int64> new_running_task;
    for (auto& id : worker.running_task()) {
      if (!removed_task_id.count(id)) {
        new_running_task.push_back(id);
      }
    }
    worker.clear_running_task();
    for (auto& id : new_running_task) {
      worker.add_running_task(id);
    }

    for (auto& id : removed_task_id) {
      worker.add_finished_task(id);
    }

    if (size > 0) {
      GetSolver()->SetResult(task_id.size(), &task_id[0], &result[0],
                             &time_usage[0], data.total_time_usage());
      if (task_pending_queue_.empty() && task_running_queue_.empty()) {
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
      auto where = req.parameters.find("startTaskId");
      int64 start_task_id = -1;
      if (where != req.parameters.end()) {
        start_task_id = std::atoll(where->second.c_str());
      }

      auto* lv = new base::ListValue();

      for (auto& iter : worker_map_) {
        auto* v = new base::DictionaryValue();
        auto& worker = iter.second;
        v->SetString("id", iter.first);
        v->SetString("status", "");
        v->SetString("runningTask", std::to_string(worker.running_task_size()));
        v->SetString("finishedTask",
                     std::to_string(worker.finished_task_size()));
        v->SetString(
            "updatedTime",
            std::to_string(base::Time::FromInternalValue(worker.updated_time())
                               .ToJsTime()));
        v->SetString("latencySum", std::to_string(worker.latency_sum()));
        v->SetString("requestCount", std::to_string(worker.request_count()));
        lv->Append(v);
      }

      base::DictionaryValue dv;
      dv.Set("nodeStatus", lv);
      dv.SetString("taskCount", std::to_string(task_map_.size()));

      if (start_task_id != -1) {
        int idx = 0;
        const int size = task_queue_.size();
        while (idx < size && task_queue_[idx] != start_task_id) {
          ++idx;
        }

        auto* lv = new base::ListValue();
        while (idx < size) {
          auto where = task_map_.find(task_queue_[idx]);
          if (where->second.status() !=
              TaskItem::TaskStatus::TaskItem_TaskStatus_DONE) {
            break;
          }
          auto* v = new base::DictionaryValue();
          v->SetString("taskId", std::to_string(task_queue_[idx]));
          v->SetString("node", std::to_string(0));
          v->SetString("timeUsage", std::to_string(where->second.time_usage()));
          lv->Append(v);
          ++idx;
        }
        dv.Set("tasks", lv);
      }

      std::string ret;
      base::JSONWriter::Write(&dv, &ret);
      rep->SetBody(ret);
    } else if (req.path == "/") {
      std::string data;
      base::FilePath filePath(
          base::UTF8ToNative(dpe_module_dir_ + "\\index.html"));
      if (!base::ReadFileToString(filePath, &data)) {
        return true;
      }
      rep->SetBody(data);
    } else if (req.path == "/jquery.min.js") {
      std::string data;
      base::FilePath filePath(
          base::UTF8ToNative(dpe_module_dir_ + "\\jquery.min.js"));
      if (!base::ReadFileToString(filePath, &data)) {
        return true;
      }
      rep->SetBody(data);
    } else if (req.path == "/Chart.bundle.js") {
      std::string data;
      base::FilePath filePath(
          base::UTF8ToNative(dpe_module_dir_ + "\\Chart.bundle.js"));
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
    for (auto& iter : task_map_) {
      TaskItem* item = master_state.add_task_item();
      item->CopyFrom(iter.second);
    }
    for (auto& iter : worker_map_) {
      WorkerStatus* worker_status = master_state.add_worker_status();
      worker_status->CopyFrom(iter.second);
    }

    google::protobuf::TextFormat::Printer printer;
    std::string data;
    printer.PrintToString(master_state, &data);

    base::FilePath file_path(
        base::UTF8ToNative(executable_dir_ + "\\state.txtproto"));
    base::WriteFile(file_path, data.c_str(), data.size());

    LOG(INFO) << "Server state saved, file size = " << data.size();
    last_save_time_ = current_time;
  }
}

void DPEMasterNode::LoadState() {
  base::FilePath file_path(
      base::UTF8ToNative(executable_dir_ + "\\state.txtproto"));
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

  std::map<int64, TaskItem> cached_status_;
  int done_count = 0;
  const int size = master_state.task_item_size();
  for (int i = 0; i < size; ++i) {
    auto& item = master_state.task_item(i);
    cached_status_[item.task_id()].CopyFrom(item);
    if (item.status() == TaskItem::TaskStatus::TaskItem_TaskStatus_DONE) {
      ++done_count;
    }
  }

  LOG(INFO) << "Load " << cached_status_.size() << " results from saved state.";
  LOG(INFO) << "Done count =  " << done_count;

  int loaded_done_count = 0;

  std::vector<int64> task_id;
  std::vector<int64> result;
  std::vector<int64> time_usage;
  std::deque<int64> new_task_pending_queue;
  for (auto& iter : task_queue_) {
    auto where = cached_status_.find(iter);
    if (where != cached_status_.end() &&
        where->second.status() ==
            TaskItem::TaskStatus::TaskItem_TaskStatus_DONE) {
      task_id.push_back(iter);
      result.push_back(where->second.result());
      time_usage.push_back(where->second.time_usage());
      task_map_[iter].CopyFrom(where->second);
      ++loaded_done_count;
    } else {
      new_task_pending_queue.push_back(iter);
    }
  }
  LOG(INFO) << "Loaded cached result count =  " << loaded_done_count;

  for (auto& iter : master_state.worker_status()) {
    auto& item = worker_map_[iter.worker_id()];
    item.CopyFrom(iter);
    item.clear_running_task();
  }

  GetSolver()->SetResult(task_id.size(), &task_id[0], &result[0],
                         &time_usage[0], 0LL);

  task_pending_queue_ = std::move(new_task_pending_queue);
}

void DPEMasterNode::SkipLoadState() {
  base::FilePath file_path(
      base::UTF8ToNative(executable_dir_ + "\\state.txtproto"));
  bool has_state = base::PathExists(file_path);
  LOG(INFO) << "Skip loading state.";
  LOG(INFO) << "State file exists: " << std::boolalpha << has_state;
}

WorkerStatus& DPEMasterNode::GetWorker(const std::string& worker_id) {
  auto where = worker_map_.find(worker_id);
  if (where != worker_map_.end()) {
    return where->second;
  }

  WorkerStatus worker;
  worker.set_worker_id(worker_id);
  worker.set_latency_sum(0LL);
  worker.set_request_count(0LL);
  worker.set_updated_time(base::Time::Now().ToInternalValue());

  worker_map_[worker_id].CopyFrom(worker);

  return worker_map_[worker_id];
}
}  // namespace dpe
