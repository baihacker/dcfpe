#ifndef DPE_DPE_H_
#define DPE_DPE_H_

#include <string>
#include <deque>
#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DPE_IMPLEMENTATION)
#define DPE_EXPORT __declspec(dllexport)
#define DPE_EXPORT_PRIVATE __declspec(dllexport)
#else
#define DPE_EXPORT __declspec(dllimport)
#define DPE_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(BASE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(DPE_IMPLEMENTATION)
#define DPE_EXPORT __attribute__((visibility("default")))
#define DPE_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define DPE_EXPORT
#define DPE_EXPORT_PRIVATE
#endif  // defined(BASE_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define DPE_EXPORT
#define DPE_EXPORT_PRIVATE
#endif

namespace dpe
{
class Solver
{
public:
  virtual void initAsMaster(std::deque<int>& taskQueue) = 0;
  virtual void initAsWorker() = 0;
  virtual void setResult(int taskId, const std::string& result) = 0;
  virtual std::string compute(int taskId) = 0;
  virtual void finish() = 0;
};

}

// Implemented by lib
//extern "C" {
DPE_EXPORT void start_dpe(dpe::Solver* solver, int argc, char* argv[]);
//}
#endif
