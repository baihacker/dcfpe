#include "remote_shell/local_server_node.h"
#include "remote_shell/proto/rs.pb.h"
#include "dpe_base/dpe_base.h"

namespace rs
{
LocalServerNode::LocalServerNode(
    const std::string& myIP):
    ServerNode(myIP),
    weakptr_factory_(this)
  {

  }

LocalServerNode::~LocalServerNode()
{
}

void LocalServerNode::setTarget(std::string& targetIp, int targetPort) {
  targetAddress = base::AddressHelper::MakeZMQTCPAddress(targetIp, targetPort);
}

static void stopImpl(LocalServerNode* node) {
  node->stop();
  base::will_quit_main_loop();
}

static void willStop(LocalServerNode* node) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(stopImpl, node));
}

static void runNextCommandImpl(LocalServerNode* node) {
  node->executeCommand();
}

static void willRunNextCommand(LocalServerNode* node) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(runNextCommandImpl, node));
}

char line[1<<16];
void LocalServerNode::executeCommand() {
  for (;;) {
    printf("rs>");
    gets(line);
    std::vector<std::string> cmds;
    int now = 0;
    const int n = strlen(line);
    while (now < n) {
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
    if (cmds.empty()) {
      printf("Command is empty!\n");
      continue;
    }

    if (handleInternalCommand(cmds)) {
      break;
    }
    
    std::string myAddress = base::AddressHelper::MakeZMQTCPAddress(myIP, port);

    ExecuteCommandRequest* ecRequest = new ExecuteCommandRequest();
    ecRequest->set_address(myAddress);
    
    ecRequest->set_cmd(cmds[0]);
    cmds.erase(cmds.begin());
    for (auto& a: cmds) ecRequest->add_args(a);
    
    msgSender = new MessageSender(targetAddress);
    
    Request req;
    req.set_allocated_execute_command(ecRequest);
    msgSender->sendRequest(req, [=](const Response& reply){
      this->handleExecuteCommandResponse(reply);
    }, 0);
    break;
  }
}

bool LocalServerNode::handleInternalCommand(const std::vector<std::string>& cmds) {
  if (cmds[0] == "exit" || cmds[0] == "q") {
    willStop(this);
    return true;
  }
  const int size = cmds.size();
  LOG(INFO)<<size;
  if (cmds[0] == "sf") {
    if (cmds.size() == 1) {
      printf("No file specified!\n");
      willRunNextCommand(this);
      return true;
    }
    for (int i = 1; i < size; ++i) {
      if (!base::PathExists(base::FilePath(base::UTF8ToNative(cmds[i])))) {
        printf("%s doesn't exist\n", cmds[i].c_str());
        willRunNextCommand(this);
        return true;
      }
    }

    FileOperationRequest* foRequest = new FileOperationRequest();
    foRequest->set_cmd("sf");
    for (int i = 1; i < size; ++i) {
      foRequest->add_args(cmds[i]);
      std::string data;
      if (!base::ReadFileToString(base::FilePath(base::UTF8ToNative(cmds[i])), &data)) {
        printf("Cannot read %s\n", cmds[i].c_str());
        willRunNextCommand(this);
        return true;
      }
      foRequest->add_args(data);
    }

    msgSender = new MessageSender(targetAddress);
    Request req;
    req.set_allocated_file_operation(foRequest);
    msgSender->sendRequest(req, [=](const Response& reply){
      this->handleFileOperationResponse(req, reply);
    }, 0);
    return true;
  }
  return false;
}

void LocalServerNode::handleFileOperationResponse(const Request& req, const Response& reply) {
  const auto& foRequest = req.file_operation();
  if (foRequest.cmd() == "sf") {
    printf(reply.error_code() == 0 ? "Succeed\n" : "failed\n");
    willRunNextCommand(this);
    return;
  }
}

void LocalServerNode::handleRequest(const Request& req, Response& reply)
{
  const auto& executeCommandRequest = req.execute_command();

  int64 srvUid = req.srv_uid();
  auto connectionId = nextConnectionId++;

  if (req.has_execute_output()) {
    const auto& result = req.execute_output();
    if (result.is_exit()) {
      printf("exit code: %d\n", result.exit_code());
      willRunNextCommand(this);
    } else {
      printf(result.output().c_str());
    }
  }

  reply.set_error_code(0);
}

void LocalServerNode::handleExecuteCommandResponse(const Response& reply) {
  if (reply.error_code() != 0) {
    printf("Failed to execute command remotely!");
    willRunNextCommand(this);
    return;
  }
  printf("Command: %s\n", reply.execute_command().command().c_str());
}
}
