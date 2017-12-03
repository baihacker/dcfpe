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
  reply.set_error_code(-1);
  if (req.has_execute_command()) {
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
      reply.set_error_code(0);
      response->set_command(cmd);
    }
  } else if (req.has_file_operation()) {
    handleFileOperation(req.file_operation(), reply);
  }
}

void RemoteServerNode::handleFileOperation(const FileOperationRequest& req, Response& reply) {
  if (req.cmd() == "sf") {
    const auto& args = req.args();
    const int size = args.size();
    for (int i = 0; i + 1 < size; i += 2) {
      auto path = base::FilePath(base::UTF8ToNative(args[i]));
      auto dest = path.BaseName();
      if (base::WriteFile(dest, args[i+1].c_str(), args[i+1].size()) == -1) {
        reply.set_error_code(-1);
        return;
      }
    }
    reply.set_error_code(0);
  }
}
}
