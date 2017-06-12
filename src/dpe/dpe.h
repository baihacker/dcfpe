#ifndef DPE_DEP_H_
#define DPE_DEP_H_

#include <string>

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

Solver* getSolver();
// Implemented by lib
void start_dpe(int argc, char* argv[]);
}

#endif
