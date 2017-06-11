#include "dpe/remote_node_impl.h"
#include "dpe/dpe_node_base.h"

namespace dpe
{
RemoteNodeImpl::RemoteNodeImpl(
RemoteNodeHandler* handler, const std::string myAddress, int connectionId) :
    isReady(false),
    nextRequestId(0),
    handler(handler),
    myAddress(myAddress),
    connectionId(connectionId),
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
  
  base::DictionaryValue req;
  req.SetString("type", "request");
  req.SetString("action", "connect");
  req.SetString("address", myAddress);
  
  std::string val;
  base::JSONWriter::Write(&req, &val);
  
  zmqClient->SendRequest(
    remoteAddress,
    val.c_str(),
    val.size(),
    base::Bind(&RemoteNodeImpl::handleConnect, weakptr_factory_.GetWeakPtr()),
    0);
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
  base::Value* v = base::JSONReader::Read(
    rep->data_.c_str(), base::JSON_ALLOW_TRAILING_COMMAS);
  std::string val;
  base::DictionaryValue* dv = NULL;
  v->GetAsDictionary(&dv);
  dv->GetString("connection_id", &val);
  remoteConnectionId = atoi(val.c_str());
  
  isReady = true;
  handler->onConnectionFinished(this, true);
}

void RemoteNodeImpl::disconnect()
{
  isReady = false;

  base::DictionaryValue req;
  req.SetString("type", "request");
  req.SetString("action", "disconnect");
  req.SetString("address", myAddress);
  
  std::string val;
  base::JSONWriter::Write(&req, &val);
  
  zmqClient->SendRequest(
    remoteAddress,
    val.c_str(),
    val.size(),
    base::Bind(&RemoteNodeImpl::handleDisconnect, weakptr_factory_.GetWeakPtr()),
    0);
}
  
void  RemoteNodeImpl::handleDisconnect(base::WeakPtr<RemoteNodeImpl> self,
            scoped_refptr<base::ZMQResponse> rep)
{
  // no need to handle it
}
  
int RemoteNodeImpl::sendRequest(base::DictionaryValue* req, base::ZMQCallBack callback)
{
  int requestId = ++nextRequestId;
  req->SetString("connection_id", base::StringPrintf("%d", connectionId));
  req->SetString("request_id", base::StringPrintf("%d", requestId));
  
  std::string val;
  base::JSONWriter::Write(req, &val);
  
  zmqClient->SendRequest(
    remoteAddress,
    val.c_str(),
    val.size(),
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

int RemoteNodeControllerImpl::getId() const
{
  return id;
}

int RemoteNodeControllerImpl::addTask(int taskId, const std::string& data,
  std::function<void (int, bool, const std::string&)> callback)
{
  base::DictionaryValue req;
  req.SetString("type", "request");
  req.SetString("action", "compute");
  req.SetString("task_id", base::StringPrintf("%d", taskId));

  if (auto* remote = pRemoteNode.get())
  {
    remote->sendRequest(&req,
        base::Bind(&RemoteNodeControllerImpl::handleAddTask, weakptr_factory_.GetWeakPtr(),
        taskId, data, callback));
  }
  return 0;
}

void RemoteNodeControllerImpl::handleAddTask(
    base::WeakPtr<RemoteNodeControllerImpl> self,
    int taskId, const std::string& data,
    std::function<void (int, bool, const std::string&)> callback,
    scoped_refptr<base::ZMQResponse> rep)
{
  if (auto* pThis = self.get())
  {
    pThis->handleAddTaskImpl(taskId, data, callback, rep);
  }
}

void RemoteNodeControllerImpl::handleAddTaskImpl(
    int taskId, const std::string& data,
    std::function<void (int, bool, const std::string&)> callback,
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

int RemoteNodeControllerImpl::finishTask(int taskId, const std::string& result)
{
  base::DictionaryValue req;
  req.SetString("type", "request");
  req.SetString("action", "finishCompute");
  req.SetString("task_id", base::StringPrintf("%d", taskId));
  req.SetString("result", result);
  if (auto* remote = pRemoteNode.get())
  {
    remote->sendRequest(&req,
      base::Bind(&RemoteNodeControllerImpl::handleFinishTask, weakptr_factory_.GetWeakPtr()));
  }
  return 0;
}
void RemoteNodeControllerImpl::handleFinishTask(base::WeakPtr<RemoteNodeControllerImpl> self,
    scoped_refptr<base::ZMQResponse> rep)
{
}
}