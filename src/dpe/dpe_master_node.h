#ifndef DPE_MASTER_NODE_H_
#define DPE_MASTER_NODE_H_

#include "dpe_base/dpe_base.h"
#include "dpe/http_server.h"
#include "dpe/proto/dpe.pb.h"
#include "dpe/zserver.h"
#include <vector>
#include <utility>
#include <queue>
#include <set>

namespace dpe {

static const int kServerPort = 3310;

class DPEMasterNode : public ZServerHandler,
                      public http::HttpReqestHandler,
                      public base::RefCounted<DPEMasterNode> {
 public:
  DPEMasterNode(const std::string& myIP, int port);
  ~DPEMasterNode();

  bool start();
  void stop();

  int handleRequest(const Request& req, Response& reply);

  bool handleRequest(const http::HttpRequest& req, http::HttpResponse* rep);

 private:
  scoped_refptr<ZServer> zserver;
  std::string myIP;
  int port;
  std::string moduleDir;
  base::WeakPtrFactory<DPEMasterNode> weakptr_factory_;

  std::deque<int64> taskQueue;
  std::set<int64> taskRunningQueue;
  std::vector<std::pair<int64, int64>> result;
};
}  // namespace dpe
#endif