#ifndef REMOTE_SHELL_REMOTE_SERVER_NODE_H_
#define REMOTE_SHELL_REMOTE_SERVER_NODE_H_

#include "remote_shell/server_node.h"
#include "remote_shell/proto/rs.pb.h"

namespace rs
{
static const int kRemoteServerPort = 3330;
class RemoteServerNode : public ServerNode, public base::RefCounted<RemoteServerNode>
{
public:
  RemoteServerNode(const std::string& myIP);
  ~RemoteServerNode();

  void handleRequest(const Request& req, Response& reply);
  void handleFileOperation(const FileOperationRequest& req, Response& reply);
private:
  base::WeakPtrFactory<RemoteServerNode>                 weakptr_factory_;
};
}
#endif