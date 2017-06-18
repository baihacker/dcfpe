#include "dpe.h"

#include <iostream>
#include <vector>

#include <windows.h>

typedef void (*DPEStartEntryType)(Solver* solver, int argc, char* argv[]);

DPEStartEntryType getEntry()
{
  HINSTANCE hDLL = LoadLibraryA("dpe.dll");
  return (DPEStartEntryType)GetProcAddress(hDLL, "start_dpe");
}

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
  int64 id;
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

  void initAsMaster(TaskAppender* taskAppender)
  {
    for (int i = 0; i < 10; ++i)
    {
      TaskData item;
      item.status = TaskData::NEW_TASK;
      item.id = i;
      item.result = 0;
      taskData.push_back(item);
      taskAppender->addTask(i);
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

  void setResult(int64 taskId, VariantsReader* result)
  {
    taskData[taskId].result = result->int64Value(0);
  }

  void compute(int64 taskId, VariantsBuilder* result)
  {
    result->appendInt64Value(taskId*taskId);
  }
  
  void finish()
  {
    int64 ans = 0;
    for (auto& t : taskData) ans += t.result;
    std::cerr << std::endl << "ans = " << ans << std::endl << std::endl;
  }
private:
  std::vector<TaskData> taskData;
};

static SolverImpl impl;

int main(int argc, char* argv[])
{
  //Sleep(5000);
  //start_dpe(&impl, argc, argv);
  getEntry()(&impl, argc, argv);
  return 0;
}