#include "dpe.h"

#include <iostream>
#include <vector>
#include <ctime>

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

  void InitMaster() {}

  int GetTaskCount() { return 30; }

  void GenerateTasks(int64* task) {
    for (int i = 0; i < 30; ++i) {
      task[i] = i;
    }
  }

  void InitWorker() {}

  void SetResult(int size, int64* task_id, int64* result, int64* time_usage,
                 int64 total_time_usage) {
    for (int i = 0; i < size; ++i) {
      std::cerr << task_id[i] << " finished. Time usage " << time_usage[i]
                << std::endl;
      ans += result[i];
    }
    std::cerr << "Time usage for batch tasks: " << total_time_usage
              << std::endl;
  }

  void Compute(int size, const int64* task_id, int64* result, int64* time_usage,
               int parallel_info) {
    for (int i = 0; i < size; ++i) {
      int start = clock();
      result[i] = Work(task_id[i]);
      time_usage[i] = (clock() - start) * 1000;
    }
  }

  int64 Work(int64 task_id) {
    //Sleep(2000);
    return task_id * task_id * task_id;
  }

  void Finish() {
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