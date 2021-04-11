#ifndef DPE_MASTER_NODE_H_
#define DPE_MASTER_NODE_H_

#include <vector>
#include <utility>
#include <queue>
#include <set>
#include <string>

#include "dpe_base/dpe_base.h"
#include "dpe/http_server.h"
#include "dpe/proto/dpe.pb.h"
#include "dpe/zserver.h"

namespace dpe {

static const int kServerPort = 3310;

class DPEMasterNode : public ZServerHandler,
                      public http::HttpReqestHandler,
                      public base::RefCounted<DPEMasterNode> {
 public:
  DPEMasterNode(const std::string& my_ip, int port);
  ~DPEMasterNode();

  bool Start();
  void Stop();

  int HandleRequest(const Request& req, Response& reply);

  bool HandleRequest(const http::HttpRequest& req, http::HttpResponse* rep);

  void SaveState(bool force_save);
  void LoadState();
  void SkipLoadState();

  WorkerStatus& GetWorker(const std::string& worker_id);

 private:
  scoped_refptr<ZServer> zserver_;
  std::string my_ip_;
  int port_;
  std::string executable_dir_;
  std::string dpe_module_dir_;
  base::WeakPtrFactory<DPEMasterNode> weakptr_factory_;

  std::vector<int64> task_queue_;
  std::deque<int64> task_pending_queue_;
  std::set<int64> task_running_queue_;
  std::map<int64, TaskItem> task_map_;
  std::map<std::string, WorkerStatus> worker_map_;
  int64 last_save_time_;
};
}  // namespace dpe
#endif