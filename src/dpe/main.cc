#include "dpe.h"

#include <iostream>
#include <vector>

#include <windows.h>

DpeStub* getDpeStub() {
  typedef DpeStub* (*GetStubType)();
  HINSTANCE hDLL = LoadLibraryA("dpe.dll");
  return ((GetStubType)GetProcAddress(hDLL, "get_stub"))();
}

DpeStub* stub = getDpeStub();

struct TaskData {
  enum TaskStatus {
    NEW_TASK,
    RUNNING,
    FINISHED,
  };
  TaskStatus status;
  int64 id;
  int64 result;
};

class SolverImpl : public Solver {
 public:
  SolverImpl() {}

  ~SolverImpl() {}

#pragma RUN_ON_MASTER_NODE
  void initAsMaster(TaskAppender* taskAppender) {
#if 0
    // Example for how to use cache
    auto cw = CacheWriter(stub->newDefaultCacheWriter());
    cw.addRef();
    cw.append(1, 1234567891234567);
    cw.release();
    auto cr = CacheReader(stub->newDefaultCacheReader());
    cr.addRef();
    std::cerr << cr.getInt64(1) << std::endl;
    cr.release();
#endif
    for (int i = 0; i < 30; ++i) {
      TaskData item;
      item.status = TaskData::NEW_TASK;
      item.id = i;
      item.result = 0;
      taskData.push_back(item);
      taskAppender->addTask(i);
    }
  }

#pragma RUN_ON_WORKER_NODE
  void initAsWorker() {
    for (int i = 0; i < 30; ++i) {
      TaskData item;
      item.status = TaskData::NEW_TASK;
      item.id = i;
      item.result = 0;
      taskData.push_back(item);
    }
  }

#pragma RUN_ON_MASTER_NODE
  void setResult(int64 taskId, void* result, int result_size, int64 timeUsage) {
    std::cerr << taskId << " finished. Timeusage " << timeUsage << std::endl;
  }

#pragma RUN_ON_WORKER_NODE
  void compute(int64 taskId) {
    Sleep(500);
  }

#pragma RUN_ON_MASTER_NODE
  void finish() {
    int64 ans = 0;
    for (auto& t : taskData) ans += t.result;
    std::cerr << std::endl << "ans = " << ans << std::endl << std::endl;
  }

 private:
  std::vector<TaskData> taskData;
};

static SolverImpl impl;

int main(int argc, char* argv[]) {
  stub->runDpe(&impl, argc, argv);
  return 0;
}