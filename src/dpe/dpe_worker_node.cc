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
      zmq_client_(base::zmq_client()) {}

DPEWorkerNode::~DPEWorkerNode() {}

bool DPEWorkerNode::Start() {
  GetSolver()->InitWorker();
  GetNextTask();
  return true;
}

void DPEWorkerNode::Stop() {}

void DPEWorkerNode::GetNextTask() {
  GetTaskRequest* get_task = new GetTaskRequest();
  const int batch_size = GetFlags().batch_size;
  const int thread_number = GetFlags().thread_number;

  get_task->set_max_task_count(batch_size == 0 ? thread_number * 3
                                               : batch_size);

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
    WillExitDpe();
    return;
  }

  Response body;
  body.ParseFromString(response->data_);
  auto& get_task = body.get_task();
  const int size = get_task.task_id_size();
  if (size == 0) {
    LOG(WARNING) << "Handle get task, no more task" << std::endl;
    WillExitDpe();
    return;
  }

  std::vector<int64> task_id;
  for (int i = 0; i < size; ++i) {
    task_id.push_back(get_task.task_id(i));
  }

  std::vector<int64> result(size, 0);
  std::vector<int64> time_usage(size, 0);

  const int64 start_time = base::Time::Now().ToInternalValue();
  GetSolver()->Compute(size, &task_id[0], &result[0], &time_usage[0],
                       GetFlags().thread_number);
  const int64 end_time = base::Time::Now().ToInternalValue();

  FinishComputeRequest* fr = new FinishComputeRequest();
  for (int i = 0; i < size; ++i) {
    TaskItem* item = fr->add_task_item();
    item->set_task_id(task_id[i]);
    item->set_result(result[i]);
    item->set_time_usage(time_usage[i]);
  }
  fr->set_total_time_usage(end_time - start_time);

  Request request;
  request.set_name("finish_compute");
  request.set_allocated_finish_compute(fr);
  SendRequest(request,
              base::Bind(&dpe::DPEWorkerNode::HandleFinishCompute, this),
              60000);
}

void DPEWorkerNode::HandleFinishCompute(
    scoped_refptr<base::ZMQResponse> response) {
  if (response->error_code_ != base::ZMQResponse::ZMQ_REP_OK) {
    LOG(WARNING) << "Handle finish compute, error: " << response->error_code_
                 << std::endl;
    WillExitDpe();
  } else {
    GetNextTask();
  }
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
