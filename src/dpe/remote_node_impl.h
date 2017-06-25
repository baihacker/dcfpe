#ifndef DPE_REMOTE_NODE_IMPL_H_
#define DPE_REMOTE_NODE_IMPL_H_

#include "dpe_base/dpe_base.h"
#include "dpe/proto/dpe.pb.h"

namespace dpe
{
static const int kDefaultRequestTimeoutInSeconds = 10;

class RemoteNodeImpl;
class DPENodeBase;

struct RemoteNodeHandler
{
  virtual int onConnectionFinished(RemoteNodeImpl* node, bool ok) = 0;
};

class RemoteNodeImpl
{
public:
  RemoteNodeImpl(RemoteNodeHandler* handler, const std::string myAddress, int64 connectionId);
  ~RemoteNodeImpl();

  void setSrvUid(int64 srvUid) {this->srvUid = srvUid;}
  int getId() const {return connectionId;};
  std::string getRemoteAddress() const {return remoteAddress;}
  bool checkIsReady() const {return isReady;}
  base::WeakPtr<RemoteNodeImpl> getWeakPtr() {return weakptr_factory_.GetWeakPtr();}

  void connectTo(const std::string& remoteAddress);

  static void  handleConnect(base::WeakPtr<RemoteNodeImpl> self,
              scoped_refptr<base::ZMQResponse> rep);

  void  handleConnectImpl(scoped_refptr<base::ZMQResponse> rep);

  void disconnect();

  int sendRequest(Request& req, base::ZMQCallBack callback, int timeout = 0);
  int sendRequest(Request& req, int timeout = 0);

  static void  handleResponse(base::WeakPtr<RemoteNodeImpl> self,
              base::ZMQCallBack callback,
              scoped_refptr<base::ZMQResponse> rep);
  void updateStatus(int64 taskId, int64 timestamp)
  {
    lastRunningTaskId = taskId;
    lastUpdateTimestamp = timestamp;
  }
  int64 getLastUpdateTimestamp() const
  {
    return lastUpdateTimestamp;
  }

  void parseRequestLatency(const Response& response);
  int64 getLatencySum() const {return latencySum;}
  int64 getRequestCount() const {return requestCount;}
private:
  int64 srvUid;
  bool isReady;
  RemoteNodeHandler* handler;

  std::string myAddress;
  std::string remoteAddress;
  const int64 connectionId;
  int64 remoteConnectionId;
  int64 nextRequestId;

  base::ZMQClient* zmqClient;

  base::WeakPtrFactory<RemoteNodeImpl>                 weakptr_factory_;

  int64 lastUpdateTimestamp;
  int64 lastRunningTaskId;
  
  int64 latencySum;
  int64 requestCount;
};

class RemoteNodeController
{
public:
  virtual void addRef() = 0;
  virtual void release() = 0;

  virtual void removeNode() = 0;
  virtual int64 getId() const = 0;

  virtual int addTask(int64 taskId, const std::string& data,
    std::function<void (int64, bool, const std::string&)> callback) = 0;

  virtual int finishTask(int64 taskId, const Variants& result, int64 timeUsage, 
    std::function<void (bool)> callback) = 0;

  virtual int updateWorkerStatus(int64 taskId, std::function<void (bool)> callback) = 0;

  virtual int64 getLastUpdateTimestamp() const = 0;
  
  virtual DPENodeBase* getLocalNode() const = 0;
  virtual RemoteNodeImpl* getRemoteNode() const = 0;
};

class RemoteNodeControllerImpl : public RemoteNodeController
{
public:
  RemoteNodeControllerImpl(
    base::WeakPtr<DPENodeBase> pLocalNode, base::WeakPtr<RemoteNodeImpl> pRemoteNode);

  void addRef();
  void release();

  int64 getId() const;
  int64 getLastUpdateTimestamp() const;
  DPENodeBase* getLocalNode() const {return pLocalNode.get();}
  RemoteNodeImpl* getRemoteNode() const {return pRemoteNode.get();}

  void removeNode();

  int addTask(int64 taskId, const std::string& data, std::function<void (int64, bool, const std::string&)> callback);
  static void handleAddTask(
    base::WeakPtr<RemoteNodeControllerImpl> self,
    int64 taskId, const std::string& data,
    std::function<void (int64, bool, const std::string&)> callback,
    scoped_refptr<base::ZMQResponse> rep);
  void handleAddTaskImpl(
    int64 taskId, const std::string& data,
    std::function<void (int64, bool, const std::string&)> callback,
    scoped_refptr<base::ZMQResponse> rep);

  int finishTask(int64 taskId, const Variants& result, int64 timeUsage, std::function<void (bool)> callback);
  static void handleFinishTask(base::WeakPtr<RemoteNodeControllerImpl> self,
    std::function<void (bool)> callback,
    scoped_refptr<base::ZMQResponse> rep);
  void handleFinishTaskImpl(
    std::function<void (bool)> callback,
    scoped_refptr<base::ZMQResponse> rep);


  int updateWorkerStatus(int64 taskId, std::function<void (bool)> callback);
  static void handleUpdateWorkerStatus(base::WeakPtr<RemoteNodeControllerImpl> self,
    std::function<void (bool)> callback,
    scoped_refptr<base::ZMQResponse> rep);
  void handleUpdateWorkerStatusImpl(
    std::function<void (bool)> callback,
    scoped_refptr<base::ZMQResponse> rep);
private:
  int64 id;
  int64 refCount;
  base::WeakPtr<DPENodeBase>    pLocalNode;
  base::WeakPtr<RemoteNodeImpl> pRemoteNode;
  base::WeakPtrFactory<RemoteNodeControllerImpl> weakptr_factory_;
};
}
#endif