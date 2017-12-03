#ifndef REMOTE_SHELL_LOCAL_SERVER_NODE_H_
#define REMOTE_SHELL_LOCAL_SERVER_NODE_H_

#include "remote_shell/server_node.h"
#include "remote_shell/proto/rs.pb.h"
#include "remote_shell/message_sender.h"
namespace rs
{
static const int kLocalServerPort = 3331;
class LocalServerNode : public ServerNode, public base::RefCounted<LocalServerNode>
{
public:
  LocalServerNode(const std::string& myIP);
  ~LocalServerNode();
  
  void setTarget(std::string& targetIp, int targetPort);

  void executeCommand();

  bool handleInternalCommand(const std::vector<std::string>& cmds);

  void handleRequest(const Request& req, Response& reply);
  
  void handleExecuteCommandResponse(int32_t zmqError, const Response& reply);
  
  void handleFileOperationResponse(int32_t zmqError, const Request& req, const Response& reply);
  
  base::WeakPtr<LocalServerNode> getWeakPtr() {return weakptr_factory_.GetWeakPtr();}
private:
  scoped_refptr<MessageSender> msgSender;
  std::string targetAddress;
  int64_t runningRequestId;
  base::WeakPtrFactory<LocalServerNode>                 weakptr_factory_;
};
}
#endif