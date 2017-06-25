#include "dpe/compute_model.h"

#include "dpe/dpe.h"
#include "dpe/dpe_internal.h"
#include "dpe/variants.h"

namespace dpe
{
SimpleMasterTaskScheduler::SimpleMasterTaskScheduler() : nextCompletedTask(0)
{
}

SimpleMasterTaskScheduler::~SimpleMasterTaskScheduler()
{
}

void SimpleMasterTaskScheduler::start()
{
  class TaskAppenderImpl : public TaskAppender
  {
  public:
    TaskAppenderImpl(std::deque<int64>& taskQueue): taskQueue(taskQueue)
    {
    }

    ~TaskAppenderImpl()
    {
    }

    void addTask(int64 taskId)
    {
      taskQueue.push_back(taskId);
    }

  private:
    std::deque<int64>& taskQueue;
  };
  TaskAppenderImpl appender(taskQueue);
  getSolver()->initAsMaster(&appender);
  for (auto& iter: taskQueue)
  {
    taskInfos.push_back({iter, -1, -1});
  }
  repeatedAction = new base::RepeatedAction(this);
  repeatedAction->Start([=](){
    refreshStatusImpl();
    }, 0, kDefaultRefreshIntervalInSeconds, -1);
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

void SimpleMasterTaskScheduler::onNodeUnavailable(int64 id)
{
  removeNodeById(id, false);
  refreshStatusImpl();
}

void SimpleMasterTaskScheduler::removeNodeById(int64 id, bool notifyRemoved)
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
  std::vector<int64> willRemove;
  for (auto& ctx: nodes)
  {
    auto now = base::Time::Now().ToInternalValue();
    auto last = ctx.node->getLastUpdateTimestamp();
    auto delta = base::TimeDelta::FromInternalValue(now - last);
    if (delta.InSeconds() > 35)
    {
      LOG(ERROR) << "Will remove node " << ctx.node->getId();
      willRemove.push_back(ctx.node->getId());
    }
    else if (ctx.status == NodeContext::READY)
    {
      if (!taskQueue.empty())
      {
        int64 nodeId = ctx.node->getId();
        int64 taskId = taskQueue.front();
        taskQueue.pop_front();

        ctx.node->addTask(taskId, "", [=](int64 taskId, bool ok, const std::string& result) {
          handleAddTaskImpl(nodeId, taskId, ok, result);
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
  for (auto id: willRemove)
  {
    removeNodeById(id, true);
  }
  if (runningCount == 0 && taskQueue.empty())
  {
    getSolver()->finish();
    if (repeatedAction)
    {
      repeatedAction->Stop();
    }
    willExitDpe();
  }
}

void  SimpleMasterTaskScheduler::handleAddTaskImpl(int64 nodeId, int64 taskId, bool ok, const std::string& result)
{
  if (!ok)
  {
    removeNodeById(nodeId, true);
  }
}

void SimpleMasterTaskScheduler::handleFinishCompute(int64 taskId, bool ok, const Variants& result, int64 timeUsage)
{
  // ok is always true
  for (auto& ctx: nodes)
  {
    if (ctx.status == NodeContext::COMPUTING_TASK && ctx.taskId == taskId)
    {
      VariantsReaderImpl vri(result);
      getSolver()->setResult(taskId, &vri, timeUsage);
      ctx.status = NodeContext::READY;
      ctx.taskId = -1;

      TaskInfo info{taskId, ctx.node->getId(), timeUsage};
      finishedTask[taskId] = info;
      flushFinishedTask();
      break;
    }
  }
  refreshStatusImpl();
}

void SimpleMasterTaskScheduler::flushFinishedTask()
{
  const int taskCount = static_cast<int>(taskInfos.size());
  while (nextCompletedTask < taskCount)
  {
    auto where = finishedTask.find(taskInfos[nextCompletedTask].taskId);
    if (where != finishedTask.end())
    {
      taskInfos[nextCompletedTask++] = std::move(where->second);
      finishedTask.erase(where);
    }
    else
    {
      break;
    }
  }
}

std::string SimpleMasterTaskScheduler::makeStatusJSON(int64 startTaskId) const
{
  auto* lv = new base::ListValue();
  for (const auto& iter: nodes)
  {
    auto* node = iter.node->getRemoteNode();
    auto* v = new base::DictionaryValue();
    v->SetString("id", std::to_string(node->getId()));
    v->SetString("status", statusToString(iter.status));
    v->SetString("taskId", std::to_string(iter.taskId));
    v->SetString("latencySum", std::to_string(node->getLatencySum()));
    v->SetString("requestCount", std::to_string(node->getRequestCount()));
    lv->Append(v);
  }

  base::DictionaryValue dv;
  dv.Set("nodeStatus", lv);

  dv.SetString("taskCount", std::to_string(taskInfos.size()));

  if (startTaskId != -1)
  {
    int l = 0, r = nextCompletedTask - 1;
    while (l <= r)
    {
      const int mid = (l + r) >> 1;
      if (taskInfos[mid].taskId >= startTaskId) r = mid - 1; else l = mid + 1;
    }
    auto* lv = new base::ListValue();
    for (int i = l; i < nextCompletedTask; ++i)
    {
      auto* v = new base::DictionaryValue();
      auto& info = taskInfos[i];
      v->SetString("taskId", std::to_string(info.taskId));
      v->SetString("node", std::to_string(info.node));
      v->SetString("timeUsage", std::to_string(info.timeUsage));
      lv->Append(v);
    }
    dv.Set("tasks", lv);
  }

  std::string ret;
  base::JSONWriter::Write(&dv, &ret);

  return ret;
}

}
