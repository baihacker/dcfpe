#ifndef DPE_DPE_NODE_BASE_H_
#define DPE_DPE_NODE_BASE_H_

#include "dpe_base/dpe_base.h"
#include "dpe/zserver.h"
#include "dpe/remote_node_impl.h"
#include "dpe/proto/dpe.pb.h"

namespace dpe
{
static const int kServerPort = 3310;
static const int kWorkerPort = 3311;

class DPENodeBase :
    public ZServerHandler,
    public RemoteNodeHandler
{
public:
  DPENodeBase(const std::string& myIP, const std::string& serverIP): 
    myIP(myIP), serverIP(serverIP), nextConnectionId(1)
  {
    
  }

  ~DPENodeBase()
  {
    if (zserver)
    {
      zserver->Stop();
      zserver = NULL;
    }
  }

  virtual bool start(int port) = 0;
  virtual void stop() = 0;

  virtual int handleConnectRequest(const std::string& address) = 0;
  virtual int handleDisconnectRequest(const std::string& address) = 0;
  virtual int onConnectionFinished(RemoteNodeImpl* node, bool ok) = 0;
  virtual int handleRequest(const Request& req, Response& reply) = 0;
  virtual void removeNode(int64 id) = 0;

protected:
  scoped_refptr<ZServer> zserver;
  std::string type;
  std::string myIP;
  std::string serverIP;

  int64 nextConnectionId;
};
}
#endif