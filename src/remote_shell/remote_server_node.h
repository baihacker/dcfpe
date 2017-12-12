#ifndef REMOTE_SHELL_REMOTE_SERVER_NODE_H_
#define REMOTE_SHELL_REMOTE_SERVER_NODE_H_

#include "remote_shell/server_node.h"
#include "remote_shell/message_sender.h"
#include "remote_shell/command_executor.h"

namespace rs {
class RemoteServerNode : public ServerNode, public base::RefCounted<RemoteServerNode> {
public:
  RemoteServerNode(const std::string& myIP);
  ~RemoteServerNode();

  bool start();
  void connectToHost(const std::string& host, int64_t sid);
  void handleCreateSessionResponse(int32_t zmqError, const Response& reply);
  void scheduleHeartBeat();
  static void checkHeartBeat(base::WeakPtr<RemoteServerNode> self);
  void checkHeartBeatImpl();

  void handleRequest(const Request& req, Response& reply);
  void handleFileOperation(const FileOperationRequest& req, Response& reply);

private:
  scoped_refptr<MessageSender> msgSender;
  std::string hostAddress;
  int64_t runningRequestId;
  int64_t sessionId;
  scoped_refptr<CommandExecutor> executor;
  base::WeakPtrFactory<RemoteServerNode>                 weakptr_factory_;
};
}
#endif