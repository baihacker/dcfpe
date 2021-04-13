#ifndef DPE_WORKER_NODE_H_
#define DPE_WORKER_NODE_H_

#include <string>

#include "dpe/http_server.h"
#include "dpe/proto/dpe.pb.h"

namespace dpe {

class DPEWorkerNode : public base::RefCounted<DPEWorkerNode> {
 public:
  DPEWorkerNode(const std::string& my_ip, const std::string& server_ip,
                int serverPort);
  ~DPEWorkerNode();

  bool Start();
  void Stop();

  void GetNextTask(int suggested_size);
  void HandleGetTask(scoped_refptr<base::ZMQResponse> response);
  void HandleFinishCompute(int suggested_size,
                           scoped_refptr<base::ZMQResponse> response);

  static void ExecuteTask(base::WeakPtr<DPEWorkerNode> self,
                          std::vector<int64> tasks);
  static void FinishExecuteTask(base::WeakPtr<DPEWorkerNode> self,
                                std::vector<int64> tasks,
                                std::vector<int64> result,
                                std::vector<int64> time_usage,
                                int64 total_time);
  void FinishExecuteTaskImpl(std::vector<int64> tasks,
                             std::vector<int64> result,
                             std::vector<int64> time_usage, int64 total_time);

  int SendRequest(Request& req, base::ZMQCallBack callback, int timeout);

  static void HandleResponse(base::WeakPtr<DPEWorkerNode> self,
                             base::ZMQCallBack callback,
                             scoped_refptr<base::ZMQResponse> rep);

 private:
  std::string my_ip_;
  int running_task_count_;
  std::string server_address_;
  base::ZMQClient* zmq_client_;
  base::WeakPtrFactory<DPEWorkerNode> weakptr_factory_;
};
}  // namespace dpe
#endif