#ifndef DPE_COMPUTE_MODEL_H_
#define DPE_COMPUTE_MODEL_H_

#include "dpe_base/dpe_base.h"
#include "dpe/remote_node_impl.h"
#include "dpe/proto/dpe.pb.h"

namespace dpe
{
class MasterTaskScheduler
{
public:
  virtual void start() = 0;
  virtual void onNodeAvailable(RemoteNodeController* node) = 0;
  virtual void onNodeUnavailable(int64 id) = 0;
  virtual void handleFinishCompute(int64 taskId, bool ok, const Variants& result) = 0;
};

struct NodeContext
{
  enum NodeStatus
  {
    READY = 0,
    ADDING_TASK = 1,
    COMPUTING_TASK = 2,
  };
  NodeStatus status;
  RemoteNodeController* node;
  int taskId;
};

class SimpleMasterTaskScheduler : public MasterTaskScheduler,
  public base::RepeatedActionHost
{
public:
  SimpleMasterTaskScheduler();

  ~SimpleMasterTaskScheduler();

  void start();
  void OnRepeatedActionFinish(base::RepeatedAction* ra){}
  void onNodeAvailable(RemoteNodeController* node);
  void onNodeUnavailable(int64 id);
  void removeNodeById(int64 id, bool notifyRemoved);
  void refreshStatusImpl();

  void handleAddTaskImpl(int64 nodeId, int64 taskId, bool ok, const std::string& result);

  void handleFinishCompute(int64 taskId, bool ok, const Variants& result);

  void prepareTaskQueue();

private:
  std::vector<NodeContext> nodes;
  
  std::deque<int64> taskQueue;
  
  scoped_refptr<base::RepeatedAction> repeatedAction;
};

}

#endif