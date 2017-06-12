#ifndef DPE_WORKER_NODE_H_
#define DPE_WORKER_NODE_H_

#include "dpe/dpe_node_base.h"

namespace dpe
{
class WorkerTaskExecuter
{
public:
  WorkerTaskExecuter();

  void start();
  void setMasterNode(RemoteNodeController* node);
  void handleCompute(base::DictionaryValue* req);
  
  static void doCompute(base::WeakPtr<WorkerTaskExecuter> self, int taskId);
  static void finishCompute(base::WeakPtr<WorkerTaskExecuter> self, int taskId, const std::string& result);
  void finishComputeImpl(int taskId, const std::string& result);
private:
  RemoteNodeController* node;
  base::WeakPtrFactory<WorkerTaskExecuter> weakptr_factory_;
};

class DPEWorkerNode:
    public DPENodeBase,
    public base::RefCounted<DPEWorkerNode>
{
public:
  DPEWorkerNode(const std::string& myIP, const std::string& serverIP);

  bool Start(int port);
  
  int handleConnectRequest(const std::string& address);

  int handleDisconnectRequest(const std::string& address);

  int onConnectionFinished(RemoteNodeImpl* node, bool ok);
  
  int handleRequest(base::DictionaryValue* req, base::DictionaryValue* reply);
  
  void removeNode(int id);
private:
  WorkerTaskExecuter taskExecuter;
  RemoteNodeImpl* remoteNode;
  int port;
  base::WeakPtrFactory<DPEWorkerNode>                 weakptr_factory_;
};
}
#endif