#ifndef DPE_WORKER_NODE_H_
#define DPE_WORKER_NODE_H_

#include "dpe/http_server.h"
#include "dpe/proto/dpe.pb.h"

namespace dpe {

class DPEWorkerNode : public base::RefCounted<DPEWorkerNode> {
 public:
  DPEWorkerNode(const std::string& server_ip, int serverPort);
  ~DPEWorkerNode();

  bool Start();
  void Stop();

  void GetNextTask();
  void HandleGetTask(scoped_refptr<base::ZMQResponse> response);
  void HandleFinishCompute(scoped_refptr<base::ZMQResponse> response);

  int SendRequest(Request& req, base::ZMQCallBack callback, int timeout);

  static void HandleResponse(base::WeakPtr<DPEWorkerNode> self,
                             base::ZMQCallBack callback,
                             scoped_refptr<base::ZMQResponse> rep);

 private:
  std::string serverAddress;
  base::ZMQClient* zmqClient;
  base::WeakPtrFactory<DPEWorkerNode> weakptr_factory_;
};
}  // namespace dpe
#endif