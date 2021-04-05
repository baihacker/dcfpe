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
  void initAsWorker() {
  }

#pragma RUN_ON_MASTER_NODE
  void setResult(int64 taskId, void* result, int result_size, int64 timeUsage) {
    std::cerr << taskId << " finished. Timeusage " << timeUsage << std::endl;
    ans += *(int64*)result;
  }

#pragma RUN_ON_WORKER_NODE
  void compute(int64 taskId) {
    Sleep(500);
  }

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