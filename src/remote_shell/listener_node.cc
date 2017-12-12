#include "remote_shell/listener_node.h"

namespace rs {
ListenerNode::ListenerNode(
    const std::string& myIP):
    ServerNode(myIP),
    nextSessionId(0),
    weakptr_factory_(this) {
}

ListenerNode::~ListenerNode() {
}

bool ListenerNode::listen() {
  return ServerNode::start(kRSListenerPort);
}

void ListenerNode::handleRequest(const Request& req, Response& reply) {
  reply.set_error_code(-1);
  if (req.has_create_session()) {
    auto sessionId = nextSessionId++;
    const auto& detail = req.create_session();
    const auto& host = detail.address();
    //std::string cmd = "rs --host=\"" + host + "\" -l=0";
    //printf(cmd.c_str());
    //system(cmd.c_str());
    auto wcmd = base::UTF8ToNative("rs --host=\"" + host + "\" -l=0 --sid=" + std::to_string(sessionId));
    STARTUPINFO                           si_ = {0};
    PROCESS_INFORMATION                   process_info_ = {0};
    CreateProcess(NULL, (wchar_t*)wcmd.c_str(),
      NULL, NULL, false, 0, NULL, NULL, &si_, &process_info_);
    reply.set_session_id(sessionId);
    reply.set_error_code(0);
  }
}
}
