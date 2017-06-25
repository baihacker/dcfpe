#include "dpe/dpe_master_node.h"

namespace dpe
{

DPEMasterNode::DPEMasterNode(
    const std::string& myIP, const std::string& serverIP):
    DPENodeBase(myIP, serverIP),
    srvUid(base::Time::Now().ToInternalValue()),
    srvUidString(std::to_string(srvUid)),
    port(kServerPort),
    weakptr_factory_(this)
  {

  }
DPEMasterNode::~DPEMasterNode()
{
  stop();
  if (scheduler)
  {
    delete scheduler;
    scheduler = NULL;
  }
}

bool DPEMasterNode::start(int port)
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

  scheduler = new SimpleMasterTaskScheduler(srvUid);
  scheduler->start();
  return true;
}

void DPEMasterNode::stop()
{
  for (auto* node : remoteNodes)
  {
    node->disconnect();
    delete node;
  }
  std::vector<RemoteNodeImpl*>().swap(remoteNodes);
  if (zserver)
  {
    zserver->Stop();
    delete zserver;
    zserver = NULL;
  }
}

int DPEMasterNode::handleConnectRequest(const std::string& address, int64& srvUid)
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
    auto* node = remoteNodes[idx];
    std::remove(remoteNodes.begin(), remoteNodes.end(), node);
    remoteNodes.pop_back();
    scheduler->onNodeUnavailable(node->getId());
    delete node;
  }

  srvUid = this->srvUid;

  auto* remoteNode = new RemoteNodeImpl(
      this,
      zserver->GetServerAddress(),
      nextConnectionId++);
  remoteNode->setSrvUid(srvUid);
  remoteNode->connectTo(address);

  remoteNode->updateStatus(-1, base::Time::Now().ToInternalValue());
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
    remoteNodes.pop_back();
    scheduler->onNodeUnavailable(node->getId());
    delete node;
  }
  return 0;
}

int DPEMasterNode::onConnectionFinished(RemoteNodeImpl* node, bool ok)
{
  if (!ok)
  {
    if (std::remove(remoteNodes.begin(), remoteNodes.end(), node) != remoteNodes.end())
    {
      remoteNodes.pop_back();
    }
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
  reply.set_srv_uid(srvUid);

  if (req.srv_uid() != srvUid)
  {
    return 0;
  }

  RemoteNodeImpl* remoteNode = NULL;
  auto id = req.connection_id();
  for (auto node: remoteNodes)
  {
    if (node->getId() == id)
    {
      remoteNode = node;
      break;
    }
  }
  if (remoteNode == NULL)
  {
    return 0;
  }

  if (req.has_finish_compute())
  {
    auto& data = req.finish_compute();
    remoteNode->updateStatus(data.task_id(), base::Time::Now().ToInternalValue());

    scheduler->handleFinishCompute(data.task_id(), true, data.result(), data.time_usage());
    reply.set_error_code(0);
  }
  else if (req.has_update_worker_status())
  {
    auto& data = req.update_worker_status();
    remoteNode->updateStatus(data.running_task_id(), base::Time::Now().ToInternalValue());
    reply.set_error_code(0);
  }
  return 0;
}

void DPEMasterNode::removeNode(int64 id)
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
    remoteNodes.pop_back();
    delete node;
  }
}

bool DPEMasterNode::handleRequest(const http::HttpRequest& req, http::HttpResponse* rep)
{
  if (req.method == "GET")
  {
    if (req.path == "/status")
    {
      if (scheduler)
      {
        auto where = req.parameters.find("startTaskId");
        int64 taskId = -1;
        if (where != req.parameters.end())
        {
          taskId = std::atoll(where->second.c_str());
        }
        rep->setBody(scheduler->makeStatusJSON(taskId));
      }
    }
    else if (req.path == "/")
    {
      std::string data;
      base::FilePath filePath(base::UTF8ToNative("index.html"));
      if (!base::ReadFileToString(filePath, &data))
      {
        return true;
      }
      rep->setBody(data);
    }
    else if (req.path == "/jquery.min.js")
    {
      std::string data;
      base::FilePath filePath(base::UTF8ToNative("jquery.min.js"));
      if (!base::ReadFileToString(filePath, &data))
      {
        return true;
      }
      rep->setBody(data);
    }
    else if (req.path == "/Chart.bundle.js")
    {
      std::string data;
      base::FilePath filePath(base::UTF8ToNative("Chart.bundle.js"));
      if (!base::ReadFileToString(filePath, &data))
      {
        return true;
      }
      rep->setBody(data);
    }
  }
  return true;
}
}
