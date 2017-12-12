#ifndef REMOTE_SHELL_LISTENER_NODE_H_
#define REMOTE_SHELL_LISTENER_NODE_H_

#include "remote_shell/server_node.h"

namespace rs {
static const int kRSListenerPort = 3330;
class ListenerNode : public ServerNode, public base::RefCounted<ListenerNode> {
public:
  ListenerNode(const std::string& myIP);
  ~ListenerNode();

  bool listen();
  void handleRequest(const Request& req, Response& reply);

private:
  int64_t nextSessionId;
  base::WeakPtrFactory<ListenerNode>                 weakptr_factory_;
};
}
#endif