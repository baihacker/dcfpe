#include "dpe/dpe_worker_node.h"

#include "dpe/dpe.h"
#include "dpe/dpe_internal.h"
#include "dpe/proto/dpe.pb.h"
#include "dpe/variants.h"

namespace dpe
{
WorkerTaskExecuter::WorkerTaskExecuter() : weakptr_factory_(this)
{
}

WorkerTaskExecuter::~WorkerTaskExecuter()
{
}

void WorkerTaskExecuter::start()
{
  getSolver()->initAsWorker();
}

void WorkerTaskExecuter::setMasterNode(RemoteNodeController* node)
{
  this->node = node;
}

void WorkerTaskExecuter::handleCompute(int taskId)
{
  base::ThreadPool::PostTask(base::ThreadPool::COMPUTE,
      FROM_HERE,
      base::Bind(&WorkerTaskExecuter::doCompute, weakptr_factory_.GetWeakPtr(), taskId));
}

void WorkerTaskExecuter::doCompute(base::WeakPtr<WorkerTaskExecuter> self, int taskId)
{
  VariantsBuilderImpl vbi;

  getSolver()->compute(taskId, &vbi);

  base::ThreadPool::PostTask(base::ThreadPool::UI,
      FROM_HERE,
      base::Bind(&WorkerTaskExecuter::finishCompute, self, taskId, vbi.getVariants()));
}

void WorkerTaskExecuter::finishCompute(base::WeakPtr<WorkerTaskExecuter> self, int taskId, const Variants& result)
{
  if (auto* pThis = self.get())
  {
    pThis->finishComputeImpl(taskId, result);
  }
}

void WorkerTaskExecuter::finishComputeImpl(int taskId, const Variants& result)
{
  node->finishTask(taskId, result, [=](bool ok){
    if (!ok)
    {
      LOG(ERROR) << "Cannot finish compute properly";
      willExitDpe();
    }
  });
}

DPEWorkerNode::DPEWorkerNode(const std::string& myIP, const std::string& serverIP):
  DPENodeBase(myIP, serverIP), port(kWorkerPort), weakptr_factory_(this), runningTaskId(-1),
  remoteNode(NULL), remoteNodeController(NULL)
{

}

DPEWorkerNode::~DPEWorkerNode()
{
  stop();
}

bool DPEWorkerNode::start(int port)
{
  this->port = port;
  zserver = new ZServer(this);
  if (!zserver->Start(myIP, port))
  {
    zserver = NULL;
    LOG(WARNING) << "Cannot start worker node.";
    LOG(WARNING) << "ip = " << myIP;
    LOG(WARNING) << "port = " << port;
    return false;
  }
  else
  {
    LOG(INFO) << "Zserver starts at: " << zserver->GetServerAddress();
  }
  taskExecuter.start();
  remoteNode = new RemoteNodeImpl(
    this,
    zserver->GetServerAddress(),
    nextConnectionId++);
  remoteNode->connectTo(base::AddressHelper::MakeZMQTCPAddress(serverIP, kServerPort));
  return true;
}

void DPEWorkerNode::stop()
{
  if (remoteNode)
  {
    remoteNode->disconnect();
    remoteNode = NULL;
  }
  if (remoteNodeController)
  {
    remoteNodeController->release();
    remoteNodeController = NULL;
  }
  if (zserver)
  {
    zserver->Stop();
    delete zserver;
    zserver = NULL;
  }
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
    willExitDpe();
  }
  return 0;
}

int DPEWorkerNode::onConnectionFinished(RemoteNodeImpl* node, bool ok)
{
  if (!ok)
  {
    delete remoteNode;
    remoteNode = NULL;
    willExitDpe();
  }
  else
  {
    if (remoteNodeController)
    {
      remoteNodeController->release();
      remoteNodeController = NULL;
    }
    remoteNodeController =
        new RemoteNodeControllerImpl(weakptr_factory_.GetWeakPtr(), node->getWeakPtr());
    remoteNodeController->addRef();
    taskExecuter.setMasterNode(remoteNodeController);

  repeatedAction = new base::RepeatedAction(this);
  repeatedAction->Start([=](){
      updateWorkerStatus();
    }, 0, 10, -1);
  }
  return 0;
}

void DPEWorkerNode::updateWorkerStatus()
{
  remoteNodeController->updateWorkerStatus(runningTaskId, [=](bool ok){
    if (!ok)
    {
      LOG(ERROR) << "Cannot send udpate worker status request";
      willExitDpe();
    }
  });
}

int DPEWorkerNode::handleRequest(const Request& req, Response& reply)
{
  if (req.has_compute())
  {
    if (runningTaskId == -1)
    {
      taskExecuter.handleCompute(req.compute().task_id());
      reply.set_error_code(0);
    }
    else
    {
      LOG(ERROR) << "Received task request while there is a running task " << runningTaskId;
      reply.set_error_code(-1);
      willExitDpe();
    }
  }
  return 0;
}

void DPEWorkerNode::removeNode(int64 id)
{
}

}
