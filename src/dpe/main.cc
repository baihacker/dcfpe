#include "dpe.h"

#include <iostream>
#include <vector>

#include <windows.h>

DpeStub* GetDpeStub() {
  typedef DpeStub* (*GetStubType)();
  HINSTANCE hDLL = LoadLibraryA("dpe.dll");
  return ((GetStubType)GetProcAddress(hDLL, "get_stub"))();
}

DpeStub* stub = GetDpeStub();

class SolverImpl : public Solver {
 public:
  SolverImpl() {}

  ~SolverImpl() {}

#pragma RUN_ON_MASTER_NODE
  void initAsMaster(TaskAppender* taskAppender) {
    for (int i = 0; i < 30; ++i) {
      taskAppender->addTask(i);
    }
  }

#pragma RUN_ON_WORKER_NODE
  void initAsWorker() {}

#pragma RUN_ON_MASTER_NODE
  void setResult(int size, int64* task_id, int64* result, int64* time_usage,
                 int64 total_time_usage) {
    for (int i = 0; i < size; ++i) {
      std::cerr << task_id[i] << " finished. Timeusage " << time_usage[i]
                << std::endl;
      ans += result[i];
    }
    std::cerr << "Timeusage for batch task" << total_time_usage << std::endl;
  }

#pragma RUN_ON_WORKER_NODE
  void compute(int size, const int64* task_id, int64* result, int64* time_usage,
               int thread_number) {
    for (int i = 0; i < size; ++i) {
      int start = clock();
      result[i] = work(task_id[i]);
      time_usage[i] = clock() - start;
    }
  }

  int64 work(int64 task_id) { return task_id * task_id; }

#pragma RUN_ON_MASTER_NODE
  void finish() {
    std::cerr << std::endl << "ans = " << ans << std::endl << std::endl;
  }

 private:
  int64 ans;
};

static SolverImpl impl;

int main(int argc, char* argv[]) {
  stub->RunDpe(&impl, argc, argv);
  return 0;
}