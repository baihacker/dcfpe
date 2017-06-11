#ifndef DPE_REMOTE_NODE_IMPL_H_
#define DPE_REMOTE_NODE_IMPL_H_

#include "dpe/dpe.h"

namespace dpe
{
class RemoteNodeImpl;
struct RemoteNodeHandler
{
  virtual int onConnectionFinished(RemoteNodeImpl* node, bool ok) = 0;
};

class RemoteNodeImpl
{
public:
  RemoteNodeImpl(RemoteNodeHandler* handler, const std::string myAddress, int connectionId);
  ~RemoteNodeImpl();
  
  int getId() const {return connectionId;};
  std::string getRemoteAddress() const {return remoteAddress;}
  bool checkIsReady() const {return isReady;}
  base::WeakPtr<RemoteNodeImpl> getWeakPtr() {return weakptr_factory_.GetWeakPtr();}

  void connectTo(const std::string& remoteAddress);

  static void  handleConnect(base::WeakPtr<RemoteNodeImpl> self,
              scoped_refptr<base::ZMQResponse> rep);

  void  handleConnectImpl(scoped_refptr<base::ZMQResponse> rep);

  void disconnect();
  
  static void  handleDisconnect(base::WeakPtr<RemoteNodeImpl> self,
              scoped_refptr<base::ZMQResponse> rep);
  
  int sendRequest(base::DictionaryValue* req, base::ZMQCallBack callback);
  
  static void  handleResponse(base::WeakPtr<RemoteNodeImpl> self,
              base::ZMQCallBack callback,
              scoped_refptr<base::ZMQResponse> rep);
private:
  bool isReady;
  RemoteNodeHandler* handler;

  std::string myAddress;
  std::string remoteAddress;
  const int connectionId;
  int remoteConnectionId;
  int nextRequestId;
  
  base::ZMQClient* zmqClient;
  
  base::WeakPtrFactory<RemoteNodeImpl>                 weakptr_factory_;
};

class DPENodeBase;
class RemoteNodeControllerImpl : public RemoteNodeController
{
public:
  RemoteNodeControllerImpl(
  base::WeakPtr<DPENodeBase> pNode, base::WeakPtr<RemoteNodeImpl> pRemoteNode);
  
  void addRef();
  void release();

  void removeNode();
  int getId() const;
  
  int addTask(int taskId, const std::string& data, std::function<void (int, bool, const std::string&)> callback);
  static void handleAddTask(
    base::WeakPtr<RemoteNodeControllerImpl> self,
    int taskId, const std::string& data,
    std::function<void (int, bool, const std::string&)> callback, scoped_refptr<base::ZMQResponse> rep);
  void handleAddTaskImpl(
    int taskId, const std::string& data,
    std::function<void (int, bool, const std::string&)> callback, scoped_refptr<base::ZMQResponse> rep);
    
  int finishTask(int taskId, const std::string& result);
  static void handleFinishTask(base::WeakPtr<RemoteNodeControllerImpl> self,
    scoped_refptr<base::ZMQResponse> rep);
private:
  base::WeakPtr<DPENodeBase>    pNode;
  base::WeakPtr<RemoteNodeImpl> pRemoteNode;
  base::WeakPtrFactory<RemoteNodeControllerImpl> weakptr_factory_;
  int id;
  int refCount;
};
}
#endif