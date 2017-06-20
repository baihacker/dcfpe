#include "dpe.h"

#include <iostream>
#include <vector>

#include <windows.h>

DpeStub* getDpeStub()
{
  typedef DpeStub* (*GetStubType)();
  HINSTANCE hDLL = LoadLibraryA("dpe.dll");
  return ((GetStubType)GetProcAddress(hDLL, "get_stub"))();
}

DpeStub* stub = getDpeStub();

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
#if 0
    auto cw = stub->newDefaultCacheWriter();
    cw->addRef();
    cw->append(1, 1234567891234567);
    cw->release();
    auto cr = stub->newDefaultCacheReader();
    cr->addRef();
    std::cerr << cr->getInt64(1) << std::endl;
    cr->release();
#endif
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
  stub->runDpe(&impl, argc, argv);
  return 0;
}