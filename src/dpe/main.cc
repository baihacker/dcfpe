#include "dpe.h"

#include <iostream>
#include <vector>

#include <windows.h>

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


class SolverImpl : public Solver
{
public:
  SolverImpl()
  {
  }

  ~SolverImpl()
  {
  }

  void initAsMaster(std::deque<int>& taskQueue)
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

  void initAsWorker()
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

  void setResult(int taskId, const std::string& result)
  {
    taskData[taskId].result = atoi(result.c_str());
  }

  std::string compute(int taskId)
  {
    char buff[256];
    sprintf(buff, "%d", taskId*taskId);
    return buff;
  }
  
  void finish()
  {
    int ans = 0;
    for (auto& t : taskData) ans += t.result;
    std::cerr << std::endl << "ans = " << ans << std::endl << std::endl;
  }
private:
  std::vector<TaskData> taskData;
};
}

dpe::SolverImpl impl;

int main(int argc, char* argv[])
{
  start_dpe(&impl, argc, argv);
  return 0;
}