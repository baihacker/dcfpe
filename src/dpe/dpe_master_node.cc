#include "dpe/dpe_master_node.h"

namespace dpe
{

DPEMasterNode::DPEMasterNode(
    const std::string& myIP, const std::string& serverIP):
    DPENodeBase(myIP, serverIP), port(kServerPort), weakptr_factory_(this)
  {
    
  }
DPEMasterNode::~DPEMasterNode()
{
  if (scheduler)
  {
    delete scheduler;
    scheduler = NULL;
  }
}

bool DPEMasterNode::Start(int port)
{
  this->port = port;
  zserver = new ZServer(this);
  if (!zserver->Start(myIP, port))
  {
    zserver = NULL;
    LOG(WARNING) << "Cannot start master node.";
    LOG(WARNING) << "ip = " << myIP;
    LOG(WARNING) << "port = " << port;
    return false;
  }
  else
  {
    LOG(INFO) << "Zserver starts at: " << zserver->GetServerAddress();
  }

  scheduler = new SimpleMasterTaskScheduler();
  scheduler->start();
  return true;
}

void DPEMasterNode::Stop()
{
  for (auto* node : remoteNodes)
  {
    node->disconnect();
    //delete node;
  }
  std::vector<RemoteNodeImpl*>().swap(remoteNodes);
  if (zserver)
  {
    zserver->Stop();
    delete zserver;
    zserver = NULL;
  }
}
  
int DPEMasterNode::handleConnectRequest(const std::string& address)
{
  const int size = static_cast<int>(remoteNodes.size());
  int idx = -1;
  for (int i = 0; i < size; ++i)
  {
    if (remoteNodes[i]->getRemoteAddress() == address)
    {
      idx = i;
      break;
    }
  }

  if (idx != -1)
  {
    RemoteNodeImpl* node = remoteNodes[idx];
    std::remove(remoteNodes.begin(), remoteNodes.end(), node);
    scheduler->onNodeUnavailable(node->getId());
    delete node;
  }

  RemoteNodeImpl* remoteNode = new RemoteNodeImpl(
      this,
      zserver->GetServerAddress(),
      nextConnectionId++);
  remoteNode->connectTo(address);
  remoteNodes.push_back(remoteNode);

  return remoteNode->getId();
}

int DPEMasterNode::handleDisconnectRequest(const std::string& address)
{
  const int size = static_cast<int>(remoteNodes.size());
  int idx = -1;
  for (int i = 0; i < size; ++i)
  {
    if (remoteNodes[i]->getRemoteAddress() == address)
    {
      idx = i;
      break;
    }
  }
  if (idx != -1)
  {
    RemoteNodeImpl* node = remoteNodes[idx];
    std::remove(remoteNodes.begin(), remoteNodes.end(), node);
    scheduler->onNodeUnavailable(node->getId());
    delete node;
  }
  return 0;
}

int DPEMasterNode::onConnectionFinished(RemoteNodeImpl* node, bool ok)
{
  if (!ok)
  {
    std::remove(remoteNodes.begin(), remoteNodes.end(), node);
    delete node;
  }
  else
  {
    scheduler->onNodeAvailable(
      new RemoteNodeControllerImpl(weakptr_factory_.GetWeakPtr(), node->getWeakPtr()));
  }
  return 0;
}
  
int DPEMasterNode::handleRequest(const Request& req, Response& reply)
{
  if (req.has_finish_compute())
  {
    auto& data = req.finish_compute();
    
    scheduler->handleFinishCompute(data.task_id(), true, data.result());
    reply.set_error_code(0);
  }
  return 0;
}
  
void DPEMasterNode::removeNode(int id)
{
  const int size = static_cast<int>(remoteNodes.size());
  int idx = -1;
  for (int i = 0; i < size; ++i)
  {
    if (remoteNodes[i]->getId() == id)
    {
      idx = i;
      break;
    }
  }
  if (idx != -1)
  {
    RemoteNodeImpl* node = remoteNodes[idx];
    std::remove(remoteNodes.begin(), remoteNodes.end(), node);
    delete node;
  }
}
}
