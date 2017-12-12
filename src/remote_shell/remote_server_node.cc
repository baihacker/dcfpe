#include "remote_shell/remote_server_node.h"

namespace rs {
RemoteServerNode::RemoteServerNode(
    const std::string& myIP):
    ServerNode(myIP),
    runningRequestId(-1),
    sessionId(-1),
    weakptr_factory_(this) {
}

RemoteServerNode::~RemoteServerNode() {
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
  req.set_name("CreateSession");
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
    printf("%s", info.c_str());
    willStop(this);
    return;
  }
  scheduleHeartBeat();
}

void RemoteServerNode::scheduleHeartBeat() {
  base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&RemoteServerNode::checkHeartBeat, weakptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(10000));
}

void RemoteServerNode::checkHeartBeat(base::WeakPtr<RemoteServerNode> self) {
  if (RemoteServerNode* pThis = self.get()) {
    self->checkHeartBeatImpl();
  }
}

void RemoteServerNode::checkHeartBeatImpl() {
  SessionHeartBeatRequest* shbRequest = new SessionHeartBeatRequest();

  Request req;
  req.set_name("SessionHeartBeat");
  req.set_session_id(sessionId);
  req.set_allocated_session_heart_beat(shbRequest);

  msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply) {
    if (zmqError == 0 && reply.error_code() == 0) {
      this->scheduleHeartBeat();
    } else {
      LOG(ERROR) << "Heart beat failed!";
      willStop(this);
    }
  }, 10*1000);
}

void RemoteServerNode::handleRequest(const Request& req, Response& reply) {
  reply.set_session_id(sessionId);
  reply.set_error_code(-1);

  if (req.session_id() != sessionId) {
    return;
  }

  if (req.has_delete_session()) {
    willStop(this);
  } else if (req.has_execute_command()) {
    const auto& executeCommandRequest = req.execute_command();

    auto connectionId = nextConnectionId++;

    ExecuteCommandResponse* response = new ExecuteCommandResponse();
    response->set_wait_for_command(executeCommandRequest.wait_for_command());
    response->set_remote_show_output(executeCommandRequest.remote_show_output());
    response->set_remote_show_error(executeCommandRequest.remote_show_error());
    response->set_local_show_output(executeCommandRequest.local_show_output());
    response->set_local_show_error(executeCommandRequest.local_show_error());

    reply.set_srv_uid(srvUid);
    reply.set_allocated_execute_command(response);
    reply.set_error_code(0);

    executor = new CommandExecutor(sessionId);

    std::string cmd = executor->execute(executeCommandRequest, req.request_id());
    if (cmd.empty()) {
      reply.set_error_code(-1);
      executor = NULL;
    } else {
      reply.set_error_code(0);
      response->set_command(cmd);
    }
  } else if (req.has_file_operation()) {
    handleFileOperation(req.file_operation(), reply);
  }
}

void RemoteServerNode::handleFileOperation(const FileOperationRequest& req, Response& reply) {
  if (req.cmd() == "fs" || req.cmd() == "fst") {
    const auto& args = req.args();
    const int size = args.size();
    for (int i = 0; i + 1 < size; i += 2) {
      auto path = base::MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(args[i])));
      if (req.cmd() == "fs") {
        path = path.BaseName();
      }
      if (base::WriteFile(path, args[i+1].c_str(), args[i+1].size()) == -1) {
        reply.set_error_code(-1);
        return;
      }
    }

    FileOperationResponse* foResponse = new FileOperationResponse();
    foResponse->set_cmd(req.cmd());
    reply.set_allocated_file_operation(foResponse);
    reply.set_error_code(0);
  } else if (req.cmd() == "fg" || req.cmd() == "fgt") {
    const auto& args = req.args();
    const int size = args.size();
    FileOperationResponse* foResponse = new FileOperationResponse();
    if (req.cmd() == "fg") {
      for (int i = 0; i < size; ++i) {
        auto path = base::MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(args[i])));
        std::string data;
        if (!base::ReadFileToString(path, &data)) {
          reply.set_error_code(-1);
          return;
        }
        foResponse->add_args(args[i]);
        foResponse->add_args(data);
      }
    } else {
      for (int i = 0; i + 1 < size; i += 2) {
        auto path = base::MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(args[i])));
        std::string data;
        if (!base::ReadFileToString(path, &data)) {
          reply.set_error_code(-1);
          return;
        }
        foResponse->add_args(args[i+1]);
        foResponse->add_args(data);
      }
    }
    foResponse->set_cmd(req.cmd());
    reply.set_allocated_file_operation(foResponse);
    reply.set_error_code(0);
  } else if (req.cmd() == "mkdir") {
    const auto& args = req.args();
    const int size = args.size();
    for (int i = 0; i < size; ++i) {
      auto path = base::MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(args[i])));
      base::File::Error err;
      if (!base::CreateDirectoryAndGetError(path, &err)) {
        reply.set_error_code(-1);
        return;
      }
    }
    FileOperationResponse* foResponse = new FileOperationResponse();
    foResponse->set_cmd(req.cmd());
    reply.set_allocated_file_operation(foResponse);
    reply.set_error_code(0);
  }
}
}
