#include "remote_shell/remote_server_node.h"
#include "remote_shell/proto/rs.pb.h"
#include "remote_shell/command_executor.h"

namespace rs
{

RemoteServerNode::RemoteServerNode(
    const std::string& myIP):
    ServerNode(myIP),
    weakptr_factory_(this)
  {

  }

RemoteServerNode::~RemoteServerNode()
{
}

void RemoteServerNode::handleRequest(const Request& req, Response& reply)
{
  const auto& executeCommandRequest = req.execute_command();

  int64 srvUid = req.srv_uid();
  auto connectionId = nextConnectionId++;

  ExecuteCommandResponse* response = new ExecuteCommandResponse();
  response->set_connection_id(connectionId);
  reply.set_srv_uid(srvUid);
  reply.set_allocated_execute_command(response);
  reply.set_error_code(0);
  
  CommandExecutor* executor = new CommandExecutor();

  std::string cmd = executor->execute(executeCommandRequest);
  if (cmd.empty()) {
    reply.set_error_code(-1);
    delete executor;
  } else {
    response->set_command(cmd);
  }
}
}
