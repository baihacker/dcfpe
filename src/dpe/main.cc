#include "dpe/dpe.h"

#include <iostream>

#include <windows.h>
#include <winsock2.h>
#include <Shlobj.h>
#pragma comment(lib, "ws2_32")

namespace dpe
{
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
    const int size = nodes.size();
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
      LOG(INFO) << "ans = " << ans;
      base::quit_main_loop();
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

static inline std::string get_iface_address()
{
  char hostname[128];
  char localHost[128][32]={{0}};
  struct hostent* temp;
  gethostname(hostname, 128);
  temp = gethostbyname( hostname );
  for(int i=0 ; temp->h_addr_list[i] != NULL && i < 1; ++i)
  {
    strcpy(localHost[i], inet_ntoa(*(struct in_addr *)temp->h_addr_list[i]));
  }
  return localHost[0];
}

void startNetwork()
{
  WSADATA wsaData;
  WORD sockVersion = MAKEWORD(2, 2);
  if (::WSAStartup(sockVersion, &wsaData) != 0)
  {
     std::cerr << "Cannot initialize wsa" << std::endl;
     exit(-1);
  }
}

void stopNetwork()
{
  ::WSACleanup();
}

std::string removePrefix(const std::string& s, char c)
{
  const int l = s.length();
  int i = 0;
  while (i < l && s[i] == c) ++i;
  return s.substr(i);
}

std::string type = "server";
std::string myAddress;
std::string serverAddress;

void run()
{
  LOG(INFO) << "running";
  LOG(INFO) << "type = " << type;
  LOG(INFO) << "myAddress = " << myAddress;
  LOG(INFO) << "serverAddress = " << serverAddress;
  
  dpe::start_dpe(type, myAddress, serverAddress);
}

int main(int argc, char* argv[])
{
  startNetwork();
  myAddress = get_iface_address();
  serverAddress = get_iface_address();
  for (int i = 1; i < argc;)
  {
    const std::string str = removePrefix(argv[i], '-');

    if (str == "t")
    {
      type = argv[i+1];
      i += 2;
    }
    else if (str == "s") {
      serverAddress = argv[i+1];
      i += 2;
    }
    else
    {
      ++i;
    }
  }
  
  base::dpe_base_main(run);

  stopNetwork();
  return 0;
}