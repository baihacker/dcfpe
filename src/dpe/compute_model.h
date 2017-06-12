#ifndef DPE_COMPUTE_MODEL_H_
#define DPE_COMPUTE_MODEL_H_

#include "dpe/dpe.h"
#include "dpe/remote_node_impl.h"

#include <iostream>
#include <windows.h>

namespace dpe
{

class RepeatedActionWrapper
{
public:
  virtual void addRef() = 0;
  virtual void release() = 0;
  virtual void start(std::function<void ()> action, int delay, int period) = 0;
  virtual void stop() = 0;
};

RepeatedActionWrapper* createRepeatedActionWrapper();

class MasterTaskScheduler
{
public:
  virtual void start() = 0;
  virtual void onNodeAvailable(RemoteNodeController* node) = 0;
  virtual void onNodeUnavailable(int id) = 0;
  virtual void handleFinishCompute(int taskId, bool ok, const std::string& result) = 0;
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

class SimpleMasterTaskScheduler : public MasterTaskScheduler
{
public:
  SimpleMasterTaskScheduler();

  ~SimpleMasterTaskScheduler();

  void start();
  void onNodeAvailable(RemoteNodeController* node);
  void onNodeUnavailable(int id);
  void removeNodeById(int id, bool notifyRemoved);
  void refreshStatusImpl();

  void handleAddTaskImpl(int nodeId, int taskId, bool ok, const std::string& result);

  void handleFinishCompute(int taskId, bool ok, const std::string& result);

  void prepareTaskQueue();

private:
  std::vector<NodeContext> nodes;
  
  std::deque<int> taskQueue;
  
  RepeatedActionWrapper* raw;
};

}

#endif