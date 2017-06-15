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

class TaskAppender
{
public:
  virtual void addTask(int taskId) = 0;
};

class Solver
{
public:
  virtual void initAsMaster(TaskAppender* taskAppender) = 0;
  virtual void initAsWorker() = 0;
  virtual void setResult(int taskId, const char* result) = 0;
  // The result buffer is always 1024 bytes.
  virtual void compute(int taskId, char* result) = 0;
  virtual void finish() = 0;
};

DPE_EXPORT void start_dpe(Solver* solver, int argc, char* argv[]);

#endif
