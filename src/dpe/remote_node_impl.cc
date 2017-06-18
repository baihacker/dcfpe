#include "dpe/remote_node_impl.h"
#include "dpe/dpe_node_base.h"

namespace dpe
{
RemoteNodeImpl::RemoteNodeImpl(
RemoteNodeHandler* handler, const std::string myAddress, int64 connectionId) :
    isReady(false),
    nextRequestId(0),
    handler(handler),
    myAddress(myAddress),
    connectionId(connectionId),
    remoteConnectionId(0),
    zmqClient(base::zmq_client()),
    weakptr_factory_(this)
{
}

RemoteNodeImpl::~RemoteNodeImpl()
{
}
void RemoteNodeImpl::connectTo(const std::string& remoteAddress)
{
  this->remoteAddress = remoteAddress;
  
  ConnectRequest* cr = new ConnectRequest();
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
  if (RemoteNodeImpl* pThis = self.get())
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
  
  isReady = true;
  handler->onConnectionFinished(this, true);
}

void RemoteNodeImpl::disconnect()
{
  isReady = false;

  DisconnectRequest* dr = new DisconnectRequest();
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

int RemoteNodeImpl::sendRequest(Request& req)
{
  return sendRequest(req, base::Bind(&NullCallback));
}

int RemoteNodeImpl::sendRequest(Request& req, base::ZMQCallBack callback)
{
  int64 requestId = ++nextRequestId;

  req.set_connection_id(remoteConnectionId);
  req.set_request_id(requestId);
  req.set_timestamp(base::Time::Now().ToInternalValue());

  std::string val;
  req.SerializeToString(&val);

  LOG(INFO) << "Send request:\n" << req.DebugString();

  zmqClient->SendRequest(
    remoteAddress,
    val.c_str(),
    static_cast<int>(val.size()),
    base::Bind(&RemoteNodeImpl::handleResponse, weakptr_factory_.GetWeakPtr(), callback),
    0);
  
  return requestId;
}
  
void  RemoteNodeImpl::handleResponse(base::WeakPtr<RemoteNodeImpl> self,
            base::ZMQCallBack callback,
            scoped_refptr<base::ZMQResponse> rep)
{
  if (RemoteNodeImpl* pThis = self.get())
  {
    Response rep1;
    rep1.ParseFromString(rep->data_);
    LOG(INFO) << "handleResponse:\n" << rep1.DebugString();
    callback.Run(rep);
  }
}

RemoteNodeControllerImpl::RemoteNodeControllerImpl(
  base::WeakPtr<DPENodeBase> pNode, base::WeakPtr<RemoteNodeImpl> pRemoteNode) :
  pNode(pNode), pRemoteNode(pRemoteNode), refCount(0), weakptr_factory_(this)
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

void RemoteNodeControllerImpl::removeNode()
{
  if (auto* local = pNode.get())
  {
    local->removeNode(id);
  }
}

int64 RemoteNodeControllerImpl::getId() const
{
  return id;
}

int RemoteNodeControllerImpl::addTask(int64 taskId, const std::string& data,
  std::function<void (int64, bool, const std::string&)> callback)
{
  ComputeRequest* cr = new ComputeRequest();
  cr->set_task_id(taskId);
  
  Request req;
  req.set_name("compute");
  req.set_allocated_compute(cr);

  if (auto* remote = pRemoteNode.get())
  {
    remote->sendRequest(req,
        base::Bind(&RemoteNodeControllerImpl::handleAddTask, weakptr_factory_.GetWeakPtr(),
        taskId, data, callback));
  }
  return 0;
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
  if (rep->error_code_ != base::ZMQResponse::ZMQ_REP_OK)
  {
    callback(taskId, false, "");
  }
  else
  {
    callback(id, true, "");
  }
}

int RemoteNodeControllerImpl::finishTask(int64 taskId, const Variants& result)
{
  FinishComputeRequest* cr = new FinishComputeRequest();
  cr->set_task_id(taskId);
  cr->set_allocated_result(new Variants(result));

  Request req;
  req.set_name("finishCompute");
  req.set_allocated_finish_compute(cr);

  if (auto* remote = pRemoteNode.get())
  {
    remote->sendRequest(req,
      base::Bind(&RemoteNodeControllerImpl::handleFinishTask, weakptr_factory_.GetWeakPtr()));
  }
  return 0;
}
void RemoteNodeControllerImpl::handleFinishTask(base::WeakPtr<RemoteNodeControllerImpl> self,
    scoped_refptr<base::ZMQResponse> rep)
{
}
}