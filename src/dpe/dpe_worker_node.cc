#include "dpe/dpe_worker_node.h"

#include "dpe_base/dpe_base.h"
#include "dpe_base/zmq_adapter.h"
#include "dpe/dpe.h"
#include "dpe/dpe_internal.h"
#include "dpe/proto/dpe.pb.h"
#include "dpe/variants.h"
#include "dpe/dpe_master_node.h"

namespace dpe {
DPEWorkerNode::DPEWorkerNode(const std::string& serverIP, int serverPort)
    : weakptr_factory_(this),
      serverAddress(
          base::AddressHelper::MakeZMQTCPAddress(serverIP, serverPort)),
      zmqClient(base::zmq_client()) {}

DPEWorkerNode::~DPEWorkerNode() {}

bool DPEWorkerNode::start() {
  Request request;
  request.set_name("get_task");
  request.set_allocated_get_task(new GetTaskRequest());
  this->sendRequest(
      request, base::Bind(&dpe::DPEWorkerNode::HandleGetTask, this), 60000);
  return true;
}

void DPEWorkerNode::HandleGetTask(scoped_refptr<base::ZMQResponse> response) {
  if (response->error_code_ != base::ZMQResponse::ZMQ_REP_OK) {
    willExitDpe();
  } else {
    Response body;
    body.ParseFromString(response->data_);
    auto& get_task = body.get_task();
    if (!get_task.has_task_id()) {
      willExitDpe();
    } else {
      getSolver()->compute(get_task.task_id());

      FinishComputeRequest* fr = new FinishComputeRequest();
      fr->set_task_id(get_task.task_id());
      Variant* v = new Variant();
      v->set_value_int64(2);
      fr->set_allocated_result(v);
      Request request;
      request.set_name("finish_compute");
      request.set_allocated_finish_compute(fr);
      this->sendRequest(
          request, base::Bind(&dpe::DPEWorkerNode::HandleFinishCompute, this),
          60000);
    }
  }
}

void DPEWorkerNode::HandleFinishCompute(
    scoped_refptr<base::ZMQResponse> response) {
  if (response->error_code_ != base::ZMQResponse::ZMQ_REP_OK) {
    willExitDpe();
  } else {
    Request request;
    request.set_name("get_task");
    request.set_allocated_get_task(new GetTaskRequest());
    this->sendRequest(
        request, base::Bind(&dpe::DPEWorkerNode::HandleGetTask, this), 60000);
  }
}

int DPEWorkerNode::sendRequest(Request& req, base::ZMQCallBack callback,
                               int timeout) {
  req.set_timestamp(base::Time::Now().ToInternalValue());

  std::string val;
  req.SerializeToString(&val);

  LOG(INFO) << "Send request:\n" << req.DebugString();

  zmqClient->SendRequest(serverAddress, val.c_str(),
                         static_cast<int>(val.size()),
                         base::Bind(&DPEWorkerNode::handleResponse,
                                    weakptr_factory_.GetWeakPtr(), callback),
                         timeout);

  return 0;
}

void DPEWorkerNode::handleResponse(base::WeakPtr<DPEWorkerNode> self,
                                   base::ZMQCallBack callback,
                                   scoped_refptr<base::ZMQResponse> rep) {
  if (auto* pThis = self.get()) {
    Response body;
    body.ParseFromString(rep->data_);
    LOG(INFO) << "handleResponse:\n" << body.DebugString();
    callback.Run(rep);
  }
}

}  // namespace dpe
