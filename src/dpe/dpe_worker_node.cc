#include "dpe/dpe_worker_node.h"

#include "dpe_base/dpe_base.h"
#include "dpe_base/zmq_adapter.h"
#include "dpe/dpe.h"
#include "dpe/dpe_internal.h"
#include "dpe/proto/dpe.pb.h"
#include "dpe/dpe_master_node.h"

namespace dpe {
DPEWorkerNode::DPEWorkerNode(const std::string& my_ip,
                             const std::string& server_ip, int server_port)
    : weakptr_factory_(this),
      my_ip_(my_ip),
      server_address_(
          base::AddressHelper::MakeZMQTCPAddress(server_ip, server_port)),
      running_task_count_(0),
      zmq_client_(base::zmq_client()) {}

DPEWorkerNode::~DPEWorkerNode() {}

bool DPEWorkerNode::Start() {
  GetSolver()->InitWorker();
  for (int i = 0; i < GetFlags().thread_number; ++i) {
    GetNextTask(1);
  }
  return true;
}

void DPEWorkerNode::Stop() {}

void DPEWorkerNode::GetNextTask(int suggested_size) {
  GetTaskRequest* get_task = new GetTaskRequest();
  const int batch_size = GetFlags().batch_size;

  get_task->set_max_task_count(batch_size > 0 ? batch_size : suggested_size);

  Request request;
  request.set_name("get_task");
  request.set_allocated_get_task(get_task);
  SendRequest(request, base::Bind(&dpe::DPEWorkerNode::HandleGetTask, this),
              5000);
}

void DPEWorkerNode::HandleGetTask(scoped_refptr<base::ZMQResponse> response) {
  if (response->error_code_ != base::ZMQResponse::ZMQ_REP_OK) {
    LOG(WARNING) << "Handle get task, error: " << response->error_code_
                 << std::endl;
    if (running_task_count_ == 0) {
      WillExitDpe();
    }
    return;
  }

  Response body;
  body.ParseFromString(response->data_);
  auto& get_task = body.get_task();
  const int size = get_task.task_id_size();
  if (size == 0) {
    LOG(WARNING) << "Handle get task, no more task" << std::endl;
    if (running_task_count_ == 0) {
      WillExitDpe();
    }
    return;
  }

  std::vector<int64> tasks;
  for (int i = 0; i < size; ++i) {
    tasks.push_back(get_task.task_id(i));
  }

  ++running_task_count_;

  base::ThreadPool::GetBlockingPool()->PostTask(
      FROM_HERE, base::Bind(DPEWorkerNode::ExecuteTask,
                            weakptr_factory_.GetWeakPtr(), tasks));
}

void DPEWorkerNode::HandleFinishCompute(
    int suggested_size, scoped_refptr<base::ZMQResponse> response) {
  if (response->error_code_ != base::ZMQResponse::ZMQ_REP_OK) {
    LOG(WARNING) << "Handle finish compute, error: " << response->error_code_
                 << std::endl;
    if (running_task_count_ == 0) {
      WillExitDpe();
    }
  } else {
    GetNextTask(suggested_size);
  }
}

void DPEWorkerNode::ExecuteTask(base::WeakPtr<DPEWorkerNode> self,
                                std::vector<int64> tasks) {
  const int size = tasks.size();
  std::vector<int64> result(size, 0);
  std::vector<int64> time_usage(size, 0);

  const int64 start_time = base::Time::Now().ToInternalValue();
  GetSolver()->Compute(size, &tasks[0], &result[0], &time_usage[0],
                       GetFlags().parallel_info);
  const int64 end_time = base::Time::Now().ToInternalValue();

  base::ThreadPool::PostTask(
      base::ThreadPool::UI, FROM_HERE,
      base::Bind(DPEWorkerNode::FinishExecuteTask, self, tasks, result,
                 time_usage, end_time - start_time));
}

void DPEWorkerNode::FinishExecuteTask(base::WeakPtr<DPEWorkerNode> self,
                                      std::vector<int64> tasks,
                                      std::vector<int64> result,
                                      std::vector<int64> time_usage,
                                      int64 total_time) {
  if (DPEWorkerNode* p_this = self.get()) {
    p_this->FinishExecuteTaskImpl(tasks, result, time_usage, total_time);
  }
}

void DPEWorkerNode::FinishExecuteTaskImpl(std::vector<int64> tasks,
                                          std::vector<int64> result,
                                          std::vector<int64> time_usage,
                                          int64 total_time) {
  --running_task_count_;

  const int size = tasks.size();
  FinishComputeRequest* fr = new FinishComputeRequest();
  for (int i = 0; i < size; ++i) {
    TaskItem* item = fr->add_task_item();
    item->set_task_id(tasks[i]);
    item->set_result(result[i]);
    item->set_time_usage(time_usage[i]);
  }
  fr->set_total_time_usage(total_time);

  int suggested_size = 1;
  if (size > 0 && GetFlags().batch_size < 0) {
    int64 expected_time = -GetFlags().batch_size;
    double actual_time = total_time * 1e-6 / size;
    if (fabs(actual_time) > 1e-2) {
      int64 can = 1. * expected_time / actual_time;
      if (can > 100) {
        suggested_size = 100;
      } else if (can <= 0) {
        suggested_size = 1;
      } else {
        suggested_size = static_cast<int>(can);
      }
    } else {
      suggested_size = size + 1;
    }
    if (suggested_size > 100) {
      suggested_size = 100;
    }
    if (suggested_size <= 0) {
      suggested_size = 1;
    }
  }
  Request request;
  request.set_name("finish_compute");
  request.set_allocated_finish_compute(fr);
  SendRequest(request,
              base::Bind(&dpe::DPEWorkerNode::HandleFinishCompute, this,
                         suggested_size),
              10000);
}

int DPEWorkerNode::SendRequest(Request& req, base::ZMQCallBack callback,
                               int timeout) {
  req.set_worker_id(my_ip_);
  req.set_request_timestamp(base::Time::Now().ToInternalValue());

  std::string val;
  req.SerializeToString(&val);

  VLOG(1) << "Send request:\n" << req.DebugString();

  zmq_client_->SendRequest(server_address_, val.c_str(),
                           static_cast<int>(val.size()),
                           base::Bind(&DPEWorkerNode::HandleResponse,
                                      weakptr_factory_.GetWeakPtr(), callback),
                           timeout);

  return 0;
}

void DPEWorkerNode::HandleResponse(base::WeakPtr<DPEWorkerNode> self,
                                   base::ZMQCallBack callback,
                                   scoped_refptr<base::ZMQResponse> rep) {
  if (auto* pThis = self.get()) {
    Response body;
    body.ParseFromString(rep->data_);
    VLOG(1) << "HandleResponse:\n" << body.DebugString();
    callback.Run(rep);
  }
}

}  // namespace dpe
