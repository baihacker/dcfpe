#ifndef REMOTE_SHELL_COMMAND_EXECUTOR_
#define REMOTE_SHELL_COMMAND_EXECUTOR_

#include "dpe_base/dpe_base.h"
#include "remote_shell/proto/rs.pb.h"
#include "remote_shell/message_sender.h"
#include "process/process.h"

namespace rs
{

class CommandExecutor : public base::RefCounted<CommandExecutor>, public process::ProcessHost
{
public:
  enum
  {
    ZSERVER_IDLE,
    ZSERVER_RUNNING,
  };
public:
  CommandExecutor();
  ~CommandExecutor();

  std::string execute(const ExecuteCommandRequest& command);

private:
  void OnStop(process::Process* p, process::ProcessContext* exit_code);
  void OnOutput(process::Process* p, bool is_std_out, const std::string& data);
  void sendBufferedOutput();
public:
  // remote message handling: bind and receive and send
  base::ZMQClient* clientStub;
  scoped_refptr<MessageSender> msgSender;

  scoped_refptr<process::Process> process;

  std::string bufferedOutput;
  int isStdout;
  int64_t lastSendTime;
  base::WeakPtrFactory<CommandExecutor>                 weakptr_factory_;
};

}
#endif