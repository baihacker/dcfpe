#include "remote_shell/server_node.h"
#include "remote_shell/proto/rs.pb.h"

namespace rs {
ServerNode::ServerNode(
    const std::string& myIP):
    myIP(myIP),
    nextConnectionId(1),
    srvUid(base::Time::Now().ToInternalValue()),
    srvUidString(std::to_string(srvUid)),
    port(0) {
}

ServerNode::~ServerNode() {
  stop();
}

bool ServerNode::start(int port) {
  this->port = port;
  zserver = new ZServer(this);
  if (!zserver->Start(myIP, port)) {
    zserver = NULL;
    LOG(WARNING) << "Cannot start server node.";
    LOG(WARNING) << "ip = " << myIP;
    LOG(WARNING) << "port = " << port;
    return false;
  } else {
    LOG(INFO) << "Zserver starts at: " << zserver->GetServerAddress();
  }
  return true;
}

void ServerNode::stop() {
  if (zserver) {
    zserver->Stop();
    zserver = NULL;
  }
}
}
