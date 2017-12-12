#ifndef REMOTE_SHELL_SERVER_NODE_H_
#define REMOTE_SHELL_SERVER_NODE_H_

#include "remote_shell/zserver.h"
#include "remote_shell/proto/rs.pb.h"

namespace rs {
class ServerNode : public ZServerHandler {
public:
  ServerNode(const std::string& myIP);
  ~ServerNode();

  bool start(int port);
  void stop();

protected:
  scoped_refptr<ZServer> zserver;
  std::string myIP;
  int port;
  int64 nextConnectionId;
  int64 srvUid;
  std::string srvUidString;
};
}
#endif