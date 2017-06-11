#ifndef DPE_WORKER_NODE_H_
#define DPE_WORKER_NODE_H_

#include "dpe/dpe.h"
#include "dpe/zserver.h"
#include "dpe/remote_node_impl.h"
#include "dpe/dpe_node_base.h"

namespace dpe
{
class TaskExecuter
{
public:
  TaskExecuter();

  void start();
  void setWorker(Worker* worker);
  void setMasterNode(RemoteNodeController* node);
  void handleCompute(base::DictionaryValue* req);
  
  static void doCompute(base::WeakPtr<TaskExecuter> self, Worker* worker, int taskId);
  static void finishCompute(base::WeakPtr<TaskExecuter> self, int taskId, const std::string& result);
  void finishComputeImpl(int taskId, const std::string& result);
private:
  Worker* worker;
  RemoteNodeController* node;
  base::WeakPtrFactory<TaskExecuter> weakptr_factory_;
};

class DPEWorkerNode:
    public DPENodeBase,
    public base::RefCounted<DPEWorkerNode>
{
public:
  DPEWorkerNode(const std::string& myIP, const std::string& serverIP);

  bool Start();
  
  int handleConnectRequest(const std::string& address);

  int handleDisconnectRequest(const std::string& address);

  int onConnectionFinished(RemoteNodeImpl* node, bool ok);
  
  int handleRequest(base::DictionaryValue* req, base::DictionaryValue* reply);
  
  void removeNode(int id);
private:
  TaskExecuter taskExecuter;
  RemoteNodeImpl* remoteNode;
  base::WeakPtrFactory<DPEWorkerNode>                 weakptr_factory_;
};
}
#endif