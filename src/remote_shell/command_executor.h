#ifndef REMOTE_SHELL_COMMAND_EXECUTOR_
#define REMOTE_SHELL_COMMAND_EXECUTOR_

#include "dpe_base/dpe_base.h"
#include "process/process.h"

#include "remote_shell/proto/rs.pb.h"
#include "remote_shell/message_sender.h"

namespace rs {
class CommandExecutor : public base::RefCounted<CommandExecutor>, public process::ProcessHost {
public:
  enum {
    ZSERVER_IDLE,
    ZSERVER_RUNNING,
  };
public:
  CommandExecutor(int64_t sessionId);
  ~CommandExecutor();

  std::string execute(const ExecuteCommandRequest& command, int64_t originalRequestId);
  base::WeakPtr<CommandExecutor> getWeakPtr() {return weakptr_factory_.GetWeakPtr();}

private:
  void OnStop(process::Process* p, process::ProcessContext* exit_code);
  void OnOutput(process::Process* p, bool is_std_out, const std::string& data);
  void sendBufferedOutput();
  void scheduleFlushOutput();
  static void flushOutput(base::WeakPtr<CommandExecutor> self);
  void flushOutputImpl();

private:
  scoped_refptr<MessageSender> msgSender;

  scoped_refptr<process::Process> process;

  int64_t originalRequestId;
  int64_t sessionId;
  int stopped;

  // Output buffer.
  std::string bufferedOutput;
  int isStdout;
  int64_t lastSendTime;

  bool waitForCommand;
  bool remoteShowOutput;
  bool remoteShowError;
  bool localShowOutput;
  bool localShowError;

  base::WeakPtrFactory<CommandExecutor>                 weakptr_factory_;
};

}
#endif