#include "dpe/compute_model.h"

#include "dpe/dpe.h"

namespace dpe
{
extern Solver* getSolver();
class RepeatedActionrapperImpl : public RepeatedActionWrapper,
  public base::RepeatedActionHost
{
public:
  RepeatedActionrapperImpl() : refCount(0), weakptr_factory_(this) {}
  void addRef()
  {
    ++refCount;
  }

  void release()
  {
    if (--refCount == 0)
    {
      delete this;
    }
  }

  void start(std::function<void ()> action, int delay, int period)
  {
    repeatedAction = new base::RepeatedAction(this);
    repeatedAction->Start(
      base::Bind(&RepeatedActionrapperImpl::doAction, action),
      base::TimeDelta::FromSeconds(delay),
      base::TimeDelta::FromSeconds(period),
      -1);
  }

  static void doAction(std::function<void ()> action)
  {
    action();
  }
  void OnRepeatedActionFinish(base::RepeatedAction* ra)
  {
    
  }
  void stop()
  {
    if (repeatedAction)
    {
      repeatedAction->Stop();
      repeatedAction = NULL;
    }
  }
private:
  int refCount;
  scoped_refptr<base::RepeatedAction> repeatedAction;
  base::WeakPtrFactory<RepeatedActionrapperImpl> weakptr_factory_;
};

SimpleMasterTaskScheduler::SimpleMasterTaskScheduler() : raw(NULL)
{
}

SimpleMasterTaskScheduler::~SimpleMasterTaskScheduler()
{
  if (raw)
  {
    raw->release();
    raw = NULL;
  }
}

class TaskAppenderImpl : public TaskAppender
{
public:
  TaskAppenderImpl(std::deque<int>& taskQueue): taskQueue(taskQueue)
  {
  }

  void addTask(int taskId)
  {
    taskQueue.push_back(taskId);
  }
private:
  std::deque<int>& taskQueue;
};

void SimpleMasterTaskScheduler::start()
{
  TaskAppenderImpl appender(taskQueue);
  getSolver()->initAsMaster(&appender);
  raw = new RepeatedActionrapperImpl();
  raw->addRef();
  raw->start([=](){refreshStatusImpl();}, 0, 10);
}

void SimpleMasterTaskScheduler::onNodeAvailable(RemoteNodeController* node)
{
  NodeContext ctx;
  ctx.status = NodeContext::READY;
  ctx.node = node;
  node->addRef();
  ctx.taskId = -1;
  nodes.push_back(ctx);
  refreshStatusImpl();
}

void SimpleMasterTaskScheduler::onNodeUnavailable(int id)
{
  removeNodeById(id, false);
  refreshStatusImpl();
}

void SimpleMasterTaskScheduler::removeNodeById(int id, bool notifyRemoved)
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

  if (notifyRemoved)
  {
    removed.node->removeNode();
  }
  
  if (removed.status == NodeContext::COMPUTING_TASK && removed.taskId != -1)
  {
    taskQueue.push_front(removed.taskId);
  }

  removed.node->release();
}

void SimpleMasterTaskScheduler::refreshStatusImpl()
{
  int runningCount = 0;
  for (auto& ctx: nodes)
  {
    if (ctx.status == NodeContext::READY)
    {
      if (!taskQueue.empty())
      {
        int nodeId = ctx.node->getId();
        int taskId = taskQueue.front();
        taskQueue.pop_front();
        
        ctx.node->addTask(taskId, "", [=](int taskId, bool ok, const std::string& result) {
          this->handleAddTaskImpl(nodeId, taskId, ok, result);
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
    getSolver()->finish();
    if (raw)
    {
      raw->stop();
    }
  }
}

void  SimpleMasterTaskScheduler::handleAddTaskImpl(int nodeId, int taskId, bool ok, const std::string& result)
{
  if (!ok)
  {
    removeNodeById(nodeId, true);
  }
}

void SimpleMasterTaskScheduler::handleFinishCompute(int taskId, bool ok, const std::string& result)
{
  // ok is always true
  for (auto& ctx: nodes)
  {
    if (ctx.status == NodeContext::COMPUTING_TASK && ctx.taskId == taskId)
    {
      getSolver()->setResult(taskId, result.c_str());
      ctx.status = NodeContext::READY;
      ctx.taskId = -1;
    }
  }
  refreshStatusImpl();
}

}
