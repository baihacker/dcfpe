#include "dpe/dpe.h"

#include <iostream>

namespace dpe
{
typedef long long int64;
struct TaskData
{
  enum TaskStatus
  {
    NEW_TASK,
    RUNNING,
    FINISHED,
  };
  TaskStatus status;
  int id;
  int64 result;
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


class MasterImpl : public Master
{
public:
  MasterImpl()
  {
  }

  void start()
  {
    prepareTaskQueue();
  }
  void onNodeAvailable(RemoteNodeController* node)
  {
    NodeContext ctx;
    ctx.status = NodeContext::READY;
    ctx.node = node;
    ctx.taskId = -1;
    nodes.push_back(ctx);
    
    refreshStatusImpl();
  }

  void onNodeUnavailable(int id)
  {
    int idx = -1;
    const int size = static_cast<int>(nodes.size());
    for (int i = 0; i < size; ++i)
    {
      if (nodes[i].node->getId() == id)
      {
        idx = i;
        break;
      }
    }
    if (idx == -1)
    {
      return;
    }
    
    NodeContext removed = nodes[idx];
    
    for (int i = idx; i + 1 < size; ++i)
    {
      nodes[i] = nodes[i+1];
    }
    nodes.pop_back();

    refreshStatusImpl();
  }

  void refreshStatusImpl()
  {
    int runningCount = 0;
    for (auto& ctx: nodes)
    {
      if (ctx.status == NodeContext::READY)
      {
        if (!taskQueue.empty())
        {
          int taskId = taskQueue.front();
          taskQueue.pop_front();

          ctx.node->addTask(taskId, "", [=](int id, bool ok, const std::string& result) {
            this->handleAddTaskImpl(id, ok, result);
          });
          ctx.status = NodeContext::COMPUTING_TASK;
          ctx.taskId = taskId;
          ++runningCount;
        }
      }
      else if (ctx.status == NodeContext::COMPUTING_TASK)
      {
        ++runningCount;
      }
    }
    if (runningCount == 0 && taskQueue.empty())
    {
      int ans = 0;
      for (auto& t : taskData) ans += t.result;
      std::cerr << "ans = " << ans << std::endl;
    }
  }

  void  handleAddTaskImpl(int id, bool ok, const std::string& result)
  {

  }

  void handleFinishCompute(int taskId, bool ok, const std::string& result)
  {
    for (auto& ctx: nodes)
    {
      if (ctx.status == NodeContext::COMPUTING_TASK && ctx.taskId == taskId)
      {
        taskData[taskId].result = atoi(result.c_str());
        ctx.status = NodeContext::READY;
        ctx.taskId = -1;
      }
    }
    refreshStatusImpl();
  }

  void prepareTaskQueue()
  {
    for (int i = 0; i < 10; ++i)
    {
      TaskData item;
      item.status = TaskData::NEW_TASK;
      item.id = i;
      item.result = 0;
      taskData.push_back(item);
      taskQueue.push_back(i);
    }
  }
private:
  std::vector<NodeContext> nodes;
  
  std::vector<TaskData> taskData;
  std::deque<int> taskQueue;
};

class WorkerImpl : public Worker
{
public:
  WorkerImpl()
  {
  }

  void start()
  {
    prepareTaskQueue();
  }
  std::string compute(int taskId)
  {
    char buff[256];
    sprintf(buff, "%d", taskId*taskId);
    return buff;
  }
  void prepareTaskQueue()
  {
    for (int i = 0; i < 10; ++i)
    {
      TaskData item;
      item.status = TaskData::NEW_TASK;
      item.id = i;
      item.result = 0;
      taskData.push_back(item);
    }
  }
private:
  std::vector<TaskData> taskData;
};

MasterImpl master;
WorkerImpl worker;
Master* getMaster()
{
  return &master;
}
Worker* getWorker()
{
  return &worker;
}
}

int main(int argc, char* argv[])
{
  dpe::start_dpe(argc, argv);
  return 0;
}