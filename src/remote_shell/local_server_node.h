#ifndef REMOTE_SHELL_LOCAL_SERVER_NODE_H_
#define REMOTE_SHELL_LOCAL_SERVER_NODE_H_

#include "remote_shell/server_node.h"
#include "remote_shell/proto/rs.pb.h"
#include "remote_shell/message_sender.h"
namespace rs
{
enum ServerStatus {
  TIMEOUT = -2,
  FAILED = -1,
  ACCEPTED = 0,
  CONNECTED = 1,
  SUCCEED = 0,
};

struct LocalServerHost {
  virtual ~LocalServerHost(){}
  virtual void onConnectStatusChanged(int newStatus) = 0;
  virtual void onCommandStatusChanged(int newStatus) = 0;
};
class LocalServerNode : public ServerNode, public base::RefCounted<LocalServerNode>
{
public:
  LocalServerNode(LocalServerHost* host, const std::string& myIP);
  ~LocalServerNode();

  bool start();

  void connectToTarget(const std::string& target);
  void handleCreateSessionResponse(int32_t zmqError, const Response& reply);

  bool executeCommandRemotely(const std::string& line);
  bool handleInternalCommand(const std::string& line, const std::vector<std::string>& cmds);

  void handleRequest(const Request& req, Response& reply) override;
  bool preHandleRequest(const Request& req, Response& reply) override;

  void handleExecuteCommandResponse(int32_t zmqError, const Response& reply);

  void handleFileOperationResponse(int32_t zmqError, const Request& req, const Response& reply);

  static void notifyCommandExecuteStatus(base::WeakPtr<LocalServerNode> pThis, int32_t newStatus);
  void willNotifyCommandExecuteStatusImpl(int32_t newStatus);
  void notifyCommandExecuteStatusImpl(int32_t newStatus);

  base::WeakPtr<LocalServerNode> getWeakPtr() {return weakptr_factory_.GetWeakPtr();}

  std::string target() const {return targetAddress;}
  bool canExitNow() const {return shoudExit;}
private:
  scoped_refptr<MessageSender> msgSender;
  LocalServerHost* host;
  std::string targetAddress;
  std::string executorAddress;
  int64_t runningRequestId;
  int64_t sessionId;
  bool shoudExit;
  base::WeakPtrFactory<LocalServerNode>                 weakptr_factory_;
};
}
#endif