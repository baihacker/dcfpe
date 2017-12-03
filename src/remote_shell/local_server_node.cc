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

    if (cmds[0] == "exit" || cmds[0] == "q") {
      willStop(this);
        return;
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
