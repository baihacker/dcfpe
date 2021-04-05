#ifndef DPE_DPE_H_
#define DPE_DPE_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DPE_IMPLEMENTATION)
#define DPE_EXPORT __declspec(dllexport)
#define DPE_EXPORT_PRIVATE __declspec(dllexport)
#else
#define DPE_EXPORT __declspec(dllimport)
#define DPE_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(DPE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(DPE_IMPLEMENTATION)
#define DPE_EXPORT __attribute__((visibility("default")))
#define DPE_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define DPE_EXPORT
#define DPE_EXPORT_PRIVATE
#endif  // defined(DPE_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define DPE_EXPORT
#define DPE_EXPORT_PRIVATE
#endif

#include <cstdint>
typedef std::int64_t int64;

class Solver;
struct DpeStub {
  void (*RunDpe)(Solver* solver, int argc, char* argv[]);
};

DPE_EXPORT DpeStub* get_stub();

// The binary compatibility issue still exists for
// TaskAppender and Solver, but we ignore them now.
class TaskAppender {
 public:
  virtual void addTask(int64 taskId) = 0;
};

#define RUN_ON_MASTER_NODE
#define RUN_ON_WORKER_NODE
class Solver {
 public:
#pragma RUN_ON_MASTER_NODE
  virtual void initAsMaster(TaskAppender* taskAppender) = 0;

#pragma RUN_ON_WORKER_NODE
  virtual void initAsWorker() = 0;

#pragma RUN_ON_MASTER_NODE
  virtual void setResult(int size, int64* taskId, int64* result,
                         int64* time_usage, int64 total_time_usage) = 0;

#pragma RUN_ON_WORKER_NODE
  virtual void compute(int size, const int64* taskId, int64* result,
                       int64* time_usage, int thread_number) = 0;

#pragma RUN_ON_MASTER_NODE
  virtual void finish() = 0;
};

#endif
