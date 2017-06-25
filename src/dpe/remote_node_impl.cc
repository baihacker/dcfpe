#include "dpe/remote_node_impl.h"
#include "dpe/dpe_node_base.h"

namespace dpe
{
RemoteNodeImpl::RemoteNodeImpl(
RemoteNodeHandler* handler, const std::string myAddress, int64 connectionId) :
    srvUid(0),
    isReady(false),
    nextRequestId(0),
    handler(handler),
    myAddress(myAddress),
    connectionId(connectionId),
    remoteConnectionId(0),
    zmqClient(base::zmq_client()),
    latencySum(0),
    requestCount(0),
    weakptr_factory_(this)
{
}

RemoteNodeImpl::~RemoteNodeImpl()
{
}

void RemoteNodeImpl::connectTo(const std::string& remoteAddress)
{
  this->remoteAddress = remoteAddress;

  auto* cr = new ConnectRequest();
  cr->set_address(myAddress);

  Request req;
  req.set_name("connect");
  req.set_allocated_connect(cr);

  sendRequest(req, base::Bind(RemoteNodeImpl::handleConnect,
    weakptr_factory_.GetWeakPtr()));
}

void  RemoteNodeImpl::handleConnect(base::WeakPtr<RemoteNodeImpl> self,
            scoped_refptr<base::ZMQResponse> rep)
{
  if (auto* pThis = self.get())
  {
    pThis->handleConnectImpl(rep);
  }
}

void  RemoteNodeImpl::handleConnectImpl(scoped_refptr<base::ZMQResponse> rep)
{
  if (rep->error_code_ != base::ZMQResponse::ZMQ_REP_OK)
  {
    handler->onConnectionFinished(this, false);
    return;
  }
  Response data;
  data.ParseFromString(rep->data_);
  remoteConnectionId = data.connect().connection_id();
  srvUid = data.srv_uid();

  isReady = true;
  handler->onConnectionFinished(this, true);
}

void RemoteNodeImpl::disconnect()
{
  isReady = false;

  auto* dr = new DisconnectRequest();
  dr->set_address(myAddress);

  Request req;
  req.set_name("disconnect");
  req.set_allocated_disconnect(dr);

  sendRequest(req);
}

static void NullCallback(scoped_refptr<base::ZMQResponse> rep)
{
  LOG(INFO) << "NullCallback";
}

int RemoteNodeImpl::sendRequest(Request& req, int timeout)
{
  return sendRequest(req, base::Bind(&NullCallback), timeout);
}

int RemoteNodeImpl::sendRequest(Request& req, base::ZMQCallBack callback, int timeout)
{
  auto requestId = ++nextRequestId;

  req.set_connection_id(remoteConnectionId);
  req.set_request_id(requestId);
  req.set_srv_uid(srvUid);
  req.set_timestamp(base::Time::Now().ToInternalValue());

  std::string val;
  req.SerializeToString(&val);

  LOG(INFO) << "Send request:\n" << req.DebugString();

  zmqClient->SendRequest(
    remoteAddress,
    val.c_str(),
    static_cast<int>(val.size()),
    base::Bind(&RemoteNodeImpl::handleResponse, weakptr_factory_.GetWeakPtr(), callback),
    timeout);

  return requestId;
}

void  RemoteNodeImpl::handleResponse(base::WeakPtr<RemoteNodeImpl> self,
            base::ZMQCallBack callback,
            scoped_refptr<base::ZMQResponse> rep)
{
  if (auto* pThis = self.get())
  {
    Response body;
    body.ParseFromString(rep->data_);
    pThis->parseRequestLatency(body);
    LOG(INFO) << "handleResponse:\n" << body.DebugString();
    callback.Run(rep);
  }
}

void RemoteNodeImpl::parseRequestLatency(const Response& response)
{
  auto t0 = response.request_timestamp();
  auto t1 = base::Time::Now().ToInternalValue();
  if (t0 > 0)
  {
    latencySum += t1 - t0;
    ++requestCount;
  }
}

RemoteNodeControllerImpl::RemoteNodeControllerImpl(
  base::WeakPtr<DPENodeBase> pLocalNode, base::WeakPtr<RemoteNodeImpl> pRemoteNode) :
  refCount(0), pLocalNode(pLocalNode), pRemoteNode(pRemoteNode), weakptr_factory_(this)
{
  id = pRemoteNode->getId();
}

void RemoteNodeControllerImpl::addRef()
{
  ++refCount;
}

void RemoteNodeControllerImpl::release()
{
  if (--refCount == 0)
  {
    delete this;
  }
}

int64 RemoteNodeControllerImpl::getId() const
{
  return id;
}

int64 RemoteNodeControllerImpl::getLastUpdateTimestamp() const
{
  if (auto* node = pRemoteNode.get())
  {
    return node->getLastUpdateTimestamp();
  }
  return -1;
}

void RemoteNodeControllerImpl::removeNode()
{
  if (auto* local = pLocalNode.get())
  {
    local->removeNode(id);
  }
}

int RemoteNodeControllerImpl::addTask(int64 taskId, const std::string& data,
  std::function<void (int64, bool, const std::string&)> callback)
{
  auto* cr = new ComputeRequest();
  cr->set_task_id(taskId);

  Request req;
  req.set_name("compute");
  req.set_allocated_compute(cr);

  if (auto* remote = pRemoteNode.get())
  {
    remote->sendRequest(req,
        base::Bind(&RemoteNodeControllerImpl::handleAddTask, weakptr_factory_.GetWeakPtr(),
        taskId, data, callback),
        kDefaultRequestTimeoutInSeconds * 1000);
    return 0;
  }
  return -1;
}

void RemoteNodeControllerImpl::handleAddTask(
    base::WeakPtr<RemoteNodeControllerImpl> self,
    int64 taskId, const std::string& data,
    std::function<void (int64, bool, const std::string&)> callback,
    scoped_refptr<base::ZMQResponse> rep)
{
  if (auto* pThis = self.get())
  {
    pThis->handleAddTaskImpl(taskId, data, callback, rep);
  }
}

void RemoteNodeControllerImpl::handleAddTaskImpl(
    int64 taskId, const std::string& data,
    std::function<void (int64, bool, const std::string&)> callback,
    scoped_refptr<base::ZMQResponse> rep)
{
  if (rep->error_code_ == base::ZMQResponse::ZMQ_REP_TIME_OUT)
  {
    LOG(ERROR) << "Add task timeout";
  }
  if (rep->error_code_ != base::ZMQResponse::ZMQ_REP_OK)
  {
    callback(taskId, false, "");
    return;
  }

  Response body;
  body.ParseFromString(rep->data_);
  callback(id, body.error_code() == 0, "");
}

int RemoteNodeControllerImpl::finishTask(int64 taskId, const Variants& result, int64 timeUsage, 
    std::function<void (bool)> callback)
{
  auto* cr = new FinishComputeRequest();
  cr->set_task_id(taskId);
  cr->set_allocated_result(new Variants(result));
  cr->set_time_usage(timeUsage);

  Request req;
  req.set_name("finishCompute");
  req.set_allocated_finish_compute(cr);

  if (auto* remote = pRemoteNode.get())
  {
    remote->sendRequest(req,
      base::Bind(&RemoteNodeControllerImpl::handleFinishTask, weakptr_factory_.GetWeakPtr(), callback),
      kDefaultRequestTimeoutInSeconds * 1000);
    return 0;
  }
  return -1;
}

void RemoteNodeControllerImpl::handleFinishTask(base::WeakPtr<RemoteNodeControllerImpl> self,
    std::function<void (bool)> callback,
    scoped_refptr<base::ZMQResponse> rep)
{
  if (auto* pThis = self.get())
  {
    pThis->handleFinishTaskImpl(callback, rep);
  }
}

void RemoteNodeControllerImpl::handleFinishTaskImpl(
    std::function<void (bool)> callback,
    scoped_refptr<base::ZMQResponse> rep)
{
  if (rep->error_code_ == base::ZMQResponse::ZMQ_REP_TIME_OUT)
  {
    LOG(ERROR) << "Finish task timeout";
  }

  if (rep->error_code_ != base::ZMQResponse::ZMQ_REP_OK)
  {
    callback(false);
    return;
  }

  Response body;
  body.ParseFromString(rep->data_);
  callback(body.error_code() == 0);
}

int RemoteNodeControllerImpl::updateWorkerStatus(int64 taskId, std::function<void (bool)> callback)
{
  auto* cr = new UpdateWorkerStatusRequest();
  cr->set_running_task_id(taskId);

  Request req;
  req.set_name("updateWorkerStatus");
  req.set_allocated_update_worker_status(cr);

  if (auto* remote = pRemoteNode.get())
  {
    remote->sendRequest(req,
      base::Bind(&RemoteNodeControllerImpl::handleUpdateWorkerStatus, weakptr_factory_.GetWeakPtr(), callback),
      kDefaultRequestTimeoutInSeconds * 1000);
    return 0;
  }
  return -1;
}

void RemoteNodeControllerImpl::handleUpdateWorkerStatus(base::WeakPtr<RemoteNodeControllerImpl> self,
    std::function<void (bool)> callback,
    scoped_refptr<base::ZMQResponse> rep)
{
  if (auto* pThis = self.get())
  {
    pThis->handleUpdateWorkerStatusImpl(callback, rep);
  }
}

void RemoteNodeControllerImpl::handleUpdateWorkerStatusImpl(
    std::function<void (bool)> callback,
    scoped_refptr<base::ZMQResponse> rep)
{
  if (rep->error_code_ == base::ZMQResponse::ZMQ_REP_TIME_OUT)
  {
    LOG(ERROR) << "Update worker status timeout";
  }

  if (rep->error_code_ != base::ZMQResponse::ZMQ_REP_OK)
  {
    callback(false);
    return;
  }

  Response body;
  body.ParseFromString(rep->data_);
  callback(body.error_code() == 0);
}
}