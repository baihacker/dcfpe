#include "dpe/dpe.h"
#include "dpe/zserver.h"
#include "dpe/remote_node_impl.h"
#include "dpe/dpe_master_node.h"
#include "dpe/dpe_worker_node.h"

namespace dpe
{
class RepeatedActionrapperImpl : public RepeatedActionWrapper,
  public base::RepeatedActionHost
{
public:
  RepeatedActionrapperImpl() : refCount(0), weakptr_factory_(this) {}
  void addRef()
  {
    ++refCount;
  }
  void release()
  {
    if (--refCount == 0)
    {
      delete this;
    }
  }
  void start(std::function<void ()> action, int delay, int period)
  {
    repeatedAction = new base::RepeatedAction(this);
    repeatedAction->Start(
      base::Bind(&RepeatedActionrapperImpl::doAction, action),
      base::TimeDelta::FromSeconds(delay),
      base::TimeDelta::FromSeconds(period),
      -1
      );
  }

  static void doAction(std::function<void ()> action)
  {
    //action();
  }
  void OnRepeatedActionFinish(base::RepeatedAction* ra)
  {
    
  }
  void stop()
  {
    if (repeatedAction)
    {
      repeatedAction->Stop();
    }
  }
private:
  int refCount;
  scoped_refptr<base::RepeatedAction> repeatedAction;
  base::WeakPtrFactory<RepeatedActionrapperImpl> weakptr_factory_;
};

RepeatedActionWrapper* createRepeatedActionWrapper()
{
  return new RepeatedActionrapperImpl();
}

scoped_refptr<DPEMasterNode> dpeMasterNode;
scoped_refptr<DPEWorkerNode> dpeWorkerNode;

void start_dpe(const std::string& type, const std::string& myIP,
  const std::string& serverIP) {
  if (type == "server")
  {
    dpeMasterNode = new DPEMasterNode(myIP, serverIP);
    dpeMasterNode->Start();
  }
  else if (type == "worker")
  {
    dpeWorkerNode = new DPEWorkerNode(myIP, serverIP);
    dpeWorkerNode->Start();
  }
  else
  {
    LOG(ERROR) << "Unknown type";
    base::quit_main_loop();
  }
}
}