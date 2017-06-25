#ifndef DPE_MASTER_NODE_H_
#define DPE_MASTER_NODE_H_

#include "dpe/dpe_node_base.h"
#include "dpe/compute_model.h"
#include "dpe/http_server.h"

namespace dpe
{
class DPEMasterNode :
    public DPENodeBase,
    public http::HttpReqestHandler,
    public base::RefCounted<DPEMasterNode>
{
public:
  DPEMasterNode(
    const std::string& myIP, const std::string& serverIP);
  ~DPEMasterNode();

  bool start(int port);
  void stop();

  int handleConnectRequest(const std::string& address);

  int handleDisconnectRequest(const std::string& address);

  int onConnectionFinished(RemoteNodeImpl* node, bool ok);

  int handleRequest(const Request& req, Response& reply);

  void removeNode(int64 id);
  
  bool handleRequest(const http::HttpRequest& req, http::HttpResponse* rep);
private:
  MasterTaskScheduler* scheduler;
  int port;
  std::vector<RemoteNodeImpl*> remoteNodes;
  base::WeakPtrFactory<DPEMasterNode>                 weakptr_factory_;
};
}
#endif