#ifndef DPE_DEP_H_
#define DPE_DEP_H_

#include <string>

namespace dpe
{
struct RemoteNode;

class RemoteNodeController
{
public:
  virtual void addRef() = 0;
  virtual void release() = 0;
  virtual void removeNode() = 0;
  virtual int getId() const = 0;
  virtual int addTask(int taskId, const std::string& data, std::function<void (int, bool, const std::string&)> callback) = 0;
  virtual int finishTask(int taskId, const std::string& result) = 0;
};

class RepeatedActionWrapper
{
public:
  virtual void addRef() = 0;
  virtual void release() = 0;
  virtual void start(std::function<void ()> action, int delay, int period) = 0;
  virtual void stop() = 0;
};

RepeatedActionWrapper* createRepeatedActionWrapper();

class Master
{
public:
  virtual void start() = 0;
  virtual void onNodeAvailable(RemoteNodeController* node) = 0;
  virtual void onNodeUnavailable(int id) = 0;
  virtual void handleFinishCompute(int taskId, bool ok, const std::string& result) = 0;
};

class Worker
{
public:
  virtual void start() = 0;
  virtual std::string compute(int taskId) = 0;
};

Master* getMaster();
Worker* getWorker();

// Implemented by lib
void start_dpe(int argc, char* argv[]);
}

#endif