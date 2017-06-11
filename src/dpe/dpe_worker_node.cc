#include "dpe/dpe_worker_node.h"

namespace dpe
{
TaskExecuter::TaskExecuter() : weakptr_factory_(this)
{
}

void TaskExecuter::start()
{
}

void TaskExecuter::setWorker(Worker* worker)
{
  this->worker = worker;
}
void TaskExecuter::setMasterNode(RemoteNodeController* node)
{
  this->node = node;
}
void TaskExecuter::handleCompute(base::DictionaryValue* req)
{
  std::string val;
  req->GetString("task_id", &val);
  int64 taskId = atoi(val.c_str());

  base::ThreadPool::PostTask(base::ThreadPool::COMPUTE,
      FROM_HERE,
      base::Bind(&TaskExecuter::doCompute, weakptr_factory_.GetWeakPtr(), worker, taskId));
}
  
void TaskExecuter::doCompute(base::WeakPtr<TaskExecuter> self, Worker* worker, int taskId)
{
  std::string result = worker->compute(taskId);
  base::ThreadPool::PostTask(base::ThreadPool::UI,
      FROM_HERE,
      base::Bind(&TaskExecuter::finishCompute, self, taskId, result));
}

void TaskExecuter::finishCompute(base::WeakPtr<TaskExecuter> self, int taskId, const std::string& result)
{
  if (auto* pThis = self.get())
  {
    pThis->finishComputeImpl(taskId, result);
  }
}

void TaskExecuter::finishComputeImpl(int taskId, const std::string& result)
{
  node->finishTask(taskId, result);
}

DPEWorkerNode::DPEWorkerNode(const std::string& myIP, const std::string& serverIP): 
  DPENodeBase(myIP, serverIP), weakptr_factory_(this)
{
  
}

bool DPEWorkerNode::Start()
{
  zserver = new ZServer(this);
  if (!zserver->Start(myIP, kWorkerPort))
  {
    zserver = NULL;
    return false;
  }
  taskExecuter.setWorker(getWorker());
  taskExecuter.start();
  remoteNode = new RemoteNodeImpl(
    this,
    zserver->GetServerAddress(),
    nextConnectionId++);
  remoteNode->connectTo(base::AddressHelper::MakeZMQTCPAddress(serverIP, kServerPort));
  return true;
}
  
int DPEWorkerNode::handleConnectRequest(const std::string& address)
{
  if (remoteNode && remoteNode->getRemoteAddress() == address)
  {
    return remoteNode->getId();
  }
  return -1;
}

int DPEWorkerNode::handleDisconnectRequest(const std::string& address)
{
  LOG(INFO) << address;
  LOG(INFO) << remoteNode->getRemoteAddress();
  if (remoteNode && remoteNode->getRemoteAddress() == address)
  {
    delete remoteNode;
    remoteNode = NULL;
    base::quit_main_loop();
  }
  return 0;
}

int DPEWorkerNode::onConnectionFinished(RemoteNodeImpl* node, bool ok)
{
  if (!ok)
  {
    delete remoteNode;
    remoteNode = NULL;
    base::quit_main_loop();
  }
  else
  {
    taskExecuter.setMasterNode(
      new RemoteNodeControllerImpl(weakptr_factory_.GetWeakPtr(), node->getWeakPtr()));
  }
  return 0;
}
  
int DPEWorkerNode::handleRequest(base::DictionaryValue* req, base::DictionaryValue* reply)
{
  std::string val;

  req->GetString("action", &val);
  
  if (val == "compute")
  {
    taskExecuter.handleCompute(req);
    reply->SetString("error_code", "0");
  }
  return 0;
}

void DPEWorkerNode::removeNode(int id)
{
}
}
