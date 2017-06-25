#ifndef DPE_COMPUTE_MODEL_H_
#define DPE_COMPUTE_MODEL_H_

#include "dpe_base/dpe_base.h"
#include "dpe/remote_node_impl.h"
#include "dpe/proto/dpe.pb.h"

namespace dpe
{
static const int kDefaultRefreshIntervalInSeconds = 10;

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
  int64 taskId;
};

static inline std::string statusToString(int status)
{
  switch (status)
  {
    case NodeContext::READY: return "ready";
    case NodeContext::ADDING_TASK: return "adding task";
    case NodeContext::COMPUTING_TASK: return "running";
    default: return "unknown";
  }
}

struct TaskInfo
{
  TaskInfo(int64 taskId = 0, int64 node = 0, int64 timeUsage = 0) :
    taskId(taskId), node(node), timeUsage(timeUsage)
    {
    }
  int64 taskId;
  int64 node;
  int64 timeUsage;
};

class MasterTaskScheduler
{
public:
  virtual void start() = 0;
  virtual void onNodeAvailable(RemoteNodeController* node) = 0;
  virtual void onNodeUnavailable(int64 id) = 0;
  virtual void handleFinishCompute(int64 taskId, bool ok, const Variants& result, int64 timeUsage) = 0;
  virtual std::string makeStatusJSON(int64 startTaskId = -1) const = 0;
};

class SimpleMasterTaskScheduler : public MasterTaskScheduler,
  public base::RepeatedActionHost
{
public:
  SimpleMasterTaskScheduler(int64 srvUid);

  ~SimpleMasterTaskScheduler();

  void start();
  void OnRepeatedActionFinish(base::RepeatedAction* ra){}
  void onNodeAvailable(RemoteNodeController* node);
  void onNodeUnavailable(int64 id);
  void removeNodeById(int64 id, bool notifyRemoved);
  void refreshStatusImpl();

  void handleAddTaskImpl(int64 nodeId, int64 taskId, bool ok, const std::string& result);

  void handleFinishCompute(int64 taskId, bool ok, const Variants& result, int64 timeUsage);

  void prepareTaskQueue();

  std::string makeStatusJSON(int64 startTaskId) const;
  void flushFinishedTask();
private:
  int64 srvUid;
  std::string srvUidString;
  std::vector<NodeContext> nodes;

  std::deque<int64> taskQueue;

  std::vector<TaskInfo> taskInfos;
  int nextCompletedTask;
  std::map<int64, TaskInfo> finishedTask;

  scoped_refptr<base::RepeatedAction> repeatedAction;
};

}

#endif