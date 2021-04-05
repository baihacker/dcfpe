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
  virtual void AddTask(int64 taskId) = 0;
};

class Solver {
 public:
  virtual void InitAsMaster(TaskAppender* taskAppender) = 0;
  virtual void InitAsWorker() = 0;
  virtual void SetResult(int size, int64* taskId, int64* result,
                         int64* time_usage, int64 total_time_usage) = 0;
  virtual void Compute(int size, const int64* taskId, int64* result,
                       int64* time_usage, int thread_number) = 0;
  virtual void Finish() = 0;
};

#endif
