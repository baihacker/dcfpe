#include "remote_shell/local_server_node.h"
#include "remote_shell/proto/rs.pb.h"
#include "dpe_base/dpe_base.h"
#include "remote_shell/listener_node.h"

namespace rs {
LocalServerNode::LocalServerNode(
    LocalServerHost* host,
    const std::string& myIP):
    host(host),
    ServerNode(myIP),
    runningRequestId(-1),
    sessionId(-1),
    shoudExit(false),
    waitForCommand(true),
    remoteShowOutput(false),
    remoteShowError(false),
    localShowOutput(true),
    localShowError(true),
    weakptr_factory_(this) {
}

LocalServerNode::~LocalServerNode() {
  if (sessionId != -1 && msgSender != NULL && !executorAddress.empty()) {
    DeleteSessionRequest* dsRequest = new DeleteSessionRequest();
    Request req;
    req.set_name("DeleteSession");
    req.set_session_id(sessionId);
    req.set_allocated_delete_session(dsRequest);
    runningRequestId = msgSender->sendRequest(req, 0);
  }
}

static const int kLocalServerStartPort = 3331;

bool LocalServerNode::start() {
  int offset = rand() % 1000;
  for (int i = kLocalServerStartPort + offset; i < 5000; ++i) {
    if (ServerNode::start(i)) {
      return true;
    }
  }
  return false;
}

void LocalServerNode::connectToTarget(const std::string& target) {
  targetAddress = base::AddressHelper::MakeZMQTCPAddress(target, kRSListenerPort);
  CreateSessionRequest* csRequest = new CreateSessionRequest();
  csRequest->set_address(zserver->GetServerAddress());

  Request req;
  req.set_name("CreateSession");
  req.set_allocated_create_session(csRequest);

  msgSender = new MessageSender(targetAddress);
  runningRequestId = msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
    this->handleCreateSessionResponse(zmqError, reply);
  }, 10*1000);
}

void LocalServerNode::handleCreateSessionResponse(int32_t zmqError, const Response& reply) {
  msgSender = NULL;
  if (zmqError == base::ZMQResponse::ZMQ_REP_TIME_OUT) {
    host->onConnectStatusChanged(ServerStatus::TIMEOUT);
  } else if (zmqError != 0 || reply.error_code() != 0) {
    host->onConnectStatusChanged(ServerStatus::FAILED);
  } else {
    sessionId = reply.session_id();
    host->onConnectStatusChanged(ServerStatus::ACCEPTED);
  }
}

bool LocalServerNode::executeCommandRemotely(const std::string& line) {
  std::vector<std::string> cmds;
  const int n = line.size();
  for (int now = 0; now < n;) {
    while (now < n && line[now] == ' ') ++now;
    if (now == n) break;

    if (line[now] == '"') {
      std::string token;
      ++now;
      for (;now < n;) {
        if (line[now] == '"') {
          ++now;
          cmds.push_back(token);
          break;
        } else if (now == '\\') {
          token += line[now++];
          if (now < n) {
            token += line[now++];
          }
        } else {
          token += line[now++];
        }
      }
    } else {
      std::string token;
      while (now < n && line[now] != ' ') {
        token += line[now++];
      }
      cmds.push_back(token);
    }
  }
  if (cmds.empty() || cmds.size() == 1 && (cmds[0] == "i" || cmds[0] == "l")) {
    printf("Command is empty!\n");
    return false;
  }

  if (handleInternalCommand(line, cmds)) {
    return true;
  }

  std::string myAddress = base::AddressHelper::MakeZMQTCPAddress(myIP, port);

  ExecuteCommandRequest* ecRequest = new ExecuteCommandRequest();
  ecRequest->set_address(myAddress);

  const int size = cmds.size();
  ecRequest->set_cmd(cmds[0]);
  for (int i = 1; i < size; ++i) {
    ecRequest->add_args(cmds[i]);
  }

  ecRequest->set_wait_for_command(waitForCommand);
  ecRequest->set_remote_show_output(remoteShowOutput);
  ecRequest->set_remote_show_error(remoteShowError);
  ecRequest->set_local_show_output(localShowOutput);
  ecRequest->set_local_show_error(localShowError);

  Request req;
  req.set_name("ExecuteCommand");
  req.set_session_id(sessionId);
  req.set_allocated_execute_command(ecRequest);
  runningRequestId = msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
    this->handleExecuteCommandResponse(zmqError, reply);
  }, 10*1000);
  return true;
}

void LocalServerNode::handleExecuteCommandResponse(int32_t zmqError, const Response& reply) {
  if (zmqError == base::ZMQResponse::ZMQ_REP_TIME_OUT) {
    willNotifyCommandExecuteStatus(ServerStatus::TIMEOUT);
    return;
  } if (zmqError != 0 || reply.error_code() != 0) {
    runningRequestId = -1;
    willNotifyCommandExecuteStatus(ServerStatus::FAILED);
    return;
  }
  printf("Remote command: %s\n", reply.execute_command().command().c_str());
  if (!waitForCommand) {
    willNotifyCommandExecuteStatus(ServerStatus::SUCCEED);
  }
}

bool LocalServerNode::handleInternalCommand(const std::string& line, const std::vector<std::string>& cmds) {
  const int size = cmds.size();
  if (cmds[0] == "exit" || cmds[0] == "q") {
    DeleteSessionRequest* dsRequest = new DeleteSessionRequest();
    Request req;
    req.set_name("DeleteSession");
    req.set_session_id(sessionId);
    req.set_allocated_delete_session(dsRequest);
    runningRequestId = msgSender->sendRequest(req, 0);

    shoudExit = true;
    return true;
  } else if (cmds[0] == "l") {
    int now = 0;
    while (line[now] != 'l') ++now;
    ++now;
    system(&line[now]);
    willNotifyCommandExecuteStatus(ServerStatus::SUCCEED);
    return true;
  } if (cmds[0] == "option") {
    for (int i = 1; i < size; ++i) {
      if (cmds[i] == "wait_for_command") {
        waitForCommand = true;
      } else if (cmds[i] == "no_wait_for_command") {
        waitForCommand = false;
      } else if (cmds[i] == "remote_show_output") {
        remoteShowOutput = true;
      } else if (cmds[i] == "no_remote_show_output") {
        remoteShowOutput = false;
      } else if (cmds[i] == "remote_show_error") {
        remoteShowError = true;
      } else if (cmds[i] == "no_remote_show_error") {
        remoteShowError = false;
      } else if (cmds[i] == "local_show_output") {
        localShowOutput = true;
      } else if (cmds[i] == "no_local_show_output") {
        localShowOutput = false;
      } else if (cmds[i] == "local_show_error") {
        localShowError = true;
      } else if (cmds[i] == "no_local_show_error") {
        localShowError = false;
      } else {
        printf("Unknown option %s", cmds[i].c_str());
      }
    }
    willNotifyCommandExecuteStatus(ServerStatus::SUCCEED);
    return true;
  } else if (cmds[0] == "fs" || cmds[0] == "fst") {
    if (size == 1) {
      printf("No file specified!\n");
      willNotifyCommandExecuteStatus(ServerStatus::FAILED);
      return true;
    }

    if (cmds[0] == "fst") {
      if (size % 2 == 0) {
        printf("Wrong number of arguments.\n");
        willNotifyCommandExecuteStatus(ServerStatus::FAILED);
        return true;
      }
    }

    if (cmds[0] == "fs") {
      for (int i = 1; i < size; ++i) {
        auto path = MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(cmds[i])));
        if (!base::PathExists(path)) {
          printf("%s doesn't exist\n", base::NativeToUTF8(path.value()).c_str());
          willNotifyCommandExecuteStatus(ServerStatus::FAILED);
          return true;
        }
      }
    } else {
      for (int i = 1; i + 1 < size; i += 2) {
        auto path = MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(cmds[i])));
        if (!base::PathExists(path)) {
          printf("%s doesn't exist\n", base::NativeToUTF8(path.value()).c_str());
          willNotifyCommandExecuteStatus(ServerStatus::FAILED);
          return true;
        }
      }
    }

    FileOperationRequest* foRequest = new FileOperationRequest();
    foRequest->set_cmd(cmds[0]);
    if (cmds[0] == "fs") {
      for (int i = 1; i < size; ++i) {
        foRequest->add_args(cmds[i]);
        std::string data;
        auto path = base::MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(cmds[i])));
        if (!base::ReadFileToString(path, &data)) {
          printf("Cannot read %s\n", base::NativeToUTF8(path.value()).c_str());
          willNotifyCommandExecuteStatus(ServerStatus::FAILED);
          return true;
        }
        foRequest->add_args(data);
      }
    } else {
      for (int i = 1; i < size; i += 2) {
        foRequest->add_args(cmds[i+1]);
        std::string data;
        auto path = base::MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(cmds[i])));
        if (!base::ReadFileToString(path, &data)) {
          printf("Cannot read %s\n", base::NativeToUTF8(path.value()).c_str());
          willNotifyCommandExecuteStatus(ServerStatus::FAILED);
          return true;
        }
        foRequest->add_args(data);
      }
    }

    Request req;
    req.set_name("FileOperation");
    req.set_session_id(sessionId);
    req.set_allocated_file_operation(foRequest);
    msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
      this->handleFileOperationResponse(zmqError, req, reply);
    }, 0);
    return true;
  } else if (cmds[0] == "fg" || cmds[0] == "fgt") {
    if (size == 1) {
      printf("No file specified!\n");
      willNotifyCommandExecuteStatus(ServerStatus::FAILED);
      return true;
    }

    if (cmds[0] == "fgt") {
      if (size % 2 == 0) {
        printf("Wrong number of arguments.\n");
        willNotifyCommandExecuteStatus(ServerStatus::FAILED);
        return true;
      }
    }

    FileOperationRequest* foRequest = new FileOperationRequest();
    foRequest->set_cmd(cmds[0]);
    for (int i = 1; i < size; ++i) {
      foRequest->add_args(cmds[i]);
    }

    Request req;
    req.set_name("FileOperation");
    req.set_session_id(sessionId);
    req.set_allocated_file_operation(foRequest);
    msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
      this->handleFileOperationResponse(zmqError, req, reply);
    }, 0);
    return true;
  } else if (cmds[0] == "mkdir") {
    if (size == 1) {
      printf("Argument missing!\n");
      willNotifyCommandExecuteStatus(ServerStatus::FAILED);
      return true;
    }

    FileOperationRequest* foRequest = new FileOperationRequest();
    foRequest->set_cmd(cmds[0]);
    for (int i = 1; i < size; ++i) {
      foRequest->add_args(cmds[i]);
    }
    Request req;
    req.set_name("FileOperation");
    req.set_session_id(sessionId);
    req.set_allocated_file_operation(foRequest);
    msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
      this->handleFileOperationResponse(zmqError, req, reply);
    }, 0);
    return true;
  }
  return false;
}

void LocalServerNode::handleFileOperationResponse(int32_t zmqError, const Request& req, const Response& reply) {
  if (zmqError == base::ZMQResponse::ZMQ_REP_TIME_OUT) {
    willNotifyCommandExecuteStatus(ServerStatus::TIMEOUT);
    return;
  } else if (zmqError != 0 || reply.error_code() != 0) {
    willNotifyCommandExecuteStatus(ServerStatus::FAILED);
    return;
  }

  const auto& foRequest = req.file_operation();
  const auto& detail = reply.file_operation();
  if (foRequest.cmd() == "fs" || foRequest.cmd() == "fst" || foRequest.cmd() == "mkdir") {
    willNotifyCommandExecuteStatus(ServerStatus::SUCCEED);
  } else if (foRequest.cmd() == "fg" || foRequest.cmd() == "fgt") {
    const auto& args = detail.args();
    const int size = args.size();
    for (int i = 0; i + 1 < size; i += 2) {
      auto path = base::MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(args[i])));
      if (foRequest.cmd() == "fg") {
        path = path.BaseName();
      }
      if (base::WriteFile(path, args[i+1].c_str(), args[i+1].size()) == -1) {
        willNotifyCommandExecuteStatus(ServerStatus::FAILED);
        return;
      }
    }
    willNotifyCommandExecuteStatus(ServerStatus::SUCCEED);
  }
}

bool LocalServerNode::preHandleRequest(const Request& req, Response& reply) {
  reply.set_session_id(sessionId);
  reply.set_error_code(-1);

  if (req.session_id() != sessionId) {
    return true;
  }

  if (req.has_session_heart_beat()) {
    const auto& detail = req.session_heart_beat();
    reply.set_error_code(0);
    return true;
  }
  return false;
}

void LocalServerNode::handleRequest(const Request& req, Response& reply) {
  reply.set_session_id(sessionId);
  reply.set_error_code(-1);

  if (req.session_id() != sessionId) {
    return;
  }

  if (req.has_execute_output()) {
    const auto& detail = req.execute_output();
    if (detail.original_request_id() != runningRequestId) {
      // Unexpected output request.
      return;
    }

    if (detail.is_exit()) {
      printf("Exit code: %d\n", detail.exit_code());
      willNotifyCommandExecuteStatus(ServerStatus::SUCCEED);
    } else {
      if (detail.is_error_output() && localShowError) {
        fprintf(stderr, "%s", detail.output().c_str());
      }
      if (!detail.is_error_output() && localShowOutput) {
        printf("%s", detail.output().c_str());
      }
    }
    reply.set_error_code(0);
  } else if (req.has_create_session()) {
    const auto& detail = req.create_session();
    executorAddress = detail.address();
    msgSender = new MessageSender(executorAddress);
    reply.set_error_code(0);
    host->onConnectStatusChanged(ServerStatus::CONNECTED);
  }
}

void LocalServerNode::notifyCommandExecuteStatus(base::WeakPtr<LocalServerNode> pThis, int32_t newStatus) {
  if (auto* self = pThis.get()) {
    self->notifyCommandExecuteStatusImpl(newStatus);
  }
}

void LocalServerNode::willNotifyCommandExecuteStatus(int32_t newStatus) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(&LocalServerNode::notifyCommandExecuteStatus, getWeakPtr(), newStatus));
}

void LocalServerNode::notifyCommandExecuteStatusImpl(int32_t newStatus) {
  host->onCommandStatusChanged(newStatus);
}
}
