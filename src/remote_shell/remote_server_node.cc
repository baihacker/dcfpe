#include "remote_shell/remote_server_node.h"
#include "remote_shell/proto/rs.pb.h"
#include "remote_shell/command_executor.h"

namespace rs
{

RemoteServerNode::RemoteServerNode(
    const std::string& myIP):
    ServerNode(myIP),
    runningRequestId(-1),
    sessionId(-1),
    weakptr_factory_(this)
  {

  }

RemoteServerNode::~RemoteServerNode()
{
}

static void stopImpl(RemoteServerNode* node) {
  node->stop();
  base::will_quit_main_loop();
}

static void willStop(RemoteServerNode* node) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(stopImpl, node));
}

static const int kRemoteServerStartPort = 3330;

bool RemoteServerNode::start() {
  int offset = rand() % 1000;
  for (int i = kRemoteServerStartPort + offset; i < 5000; ++i) {
    if (ServerNode::start(i)) {
      return true;
    }
  }
  return false;
}

void RemoteServerNode::connectToHost(const std::string& host, int64_t sid) {
  hostAddress = host;
  sessionId = sid;
  CreateSessionRequest* csRequest = new CreateSessionRequest();
  csRequest->set_address(zserver->GetServerAddress());
  
  Request req;
  req.set_session_id(sessionId);
  req.set_allocated_create_session(csRequest);

  msgSender = new MessageSender(hostAddress);
  runningRequestId = msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
    this->handleCreateSessionResponse(zmqError, reply);
  }, 10*1000);
}

void RemoteServerNode::handleCreateSessionResponse(int32_t zmqError, const Response& reply) {
  if (zmqError != 0 || reply.error_code() != 0) {
    std::string info = "Cannot connect to " + hostAddress + "!\n";
    printf(info.c_str());
    willStop(this);
    return;
  }
}

void RemoteServerNode::handleRequest(const Request& req, Response& reply)
{
  reply.set_session_id(sessionId);
  reply.set_error_code(-1);
  if (req.has_execute_command()) {
    const auto& executeCommandRequest = req.execute_command();

    auto connectionId = nextConnectionId++;

    ExecuteCommandResponse* response = new ExecuteCommandResponse();
    reply.set_srv_uid(srvUid);
    reply.set_allocated_execute_command(response);
    reply.set_error_code(0);
    
    CommandExecutor* executor = new CommandExecutor(sessionId);

    std::string cmd = executor->execute(executeCommandRequest, req.request_id());
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
  if (req.cmd() == "fs") {
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
