#include "remote_shell/command_executor.h"

namespace rs {
CommandExecutor::CommandExecutor(int64_t sessionId):
  originalRequestId(-1),
  sessionId(sessionId),
  stopped(0),
  isStdout(1),
  lastSendTime(0),
  waitForCommand(true),
  remoteShowOutput(false),
  remoteShowError(false),
  localShowOutput(true),
  localShowError(true),
  weakptr_factory_(this) {
}

CommandExecutor::~CommandExecutor() {
}

std::string CommandExecutor::execute(const ExecuteCommandRequest& command, int64_t originalRequestId) {
  this->originalRequestId = originalRequestId;

  waitForCommand = command.wait_for_command();
  remoteShowOutput = command.remote_show_output();
  remoteShowError = command.remote_show_error();
  localShowOutput = command.local_show_output();
  localShowError = command.local_show_error();

  msgSender = new MessageSender(command.address());

  base::FilePath imagePath(base::UTF8ToNative("C:\\Windows\\System32\\cmd.exe"));
  std::string arg = command.cmd();
  for (auto& iter: command.args()) {
    arg = arg + " " + iter;
  }

  if (!waitForCommand) {
    std::string cmd = "C:\\Windows\\System32\\cmd.exe /C " + arg;
    if (!remoteShowOutput) {
      cmd += " 1>nul";
    }
    if (!remoteShowError) {
      cmd += " 2>nul";
    }
    auto wcmd = base::UTF8ToNative(cmd);
    STARTUPINFO                           si_ = {0};
    PROCESS_INFORMATION                   process_info_ = {0};
    if (CreateProcess(NULL, (wchar_t*)wcmd.c_str(),
      NULL, NULL, false, 0, NULL, NULL, &si_, &process_info_)) {
        return cmd;
    } else {
      return "";
    }
  }

  process = new process::Process(this);
  auto& option = process->GetProcessOption();
  option.allow_sub_process_breakaway_job_ = true;
  option.image_path_ = imagePath;
  option.argument_list_r_.push_back(base::UTF8ToNative("/C"));
  option.argument_list_r_.push_back(base::UTF8ToNative(arg));
  if (!remoteShowOutput && !localShowOutput) {
    option.argument_list_r_.push_back(base::UTF8ToNative("1>nul"));
  }
  if (!remoteShowError && !localShowError) {
    option.argument_list_r_.push_back(base::UTF8ToNative("2>nul"));
  }
  option.redirect_std_inout_ = remoteShowOutput || localShowOutput;


  if (!process->Start()) {
    return "";
  }

  lastSendTime = base::Time::Now().ToInternalValue();
  scheduleFlushOutput();
  return base::NativeToUTF8(process->GetProcessContext()->cmd_line_);
}

void CommandExecutor::OnStop(process::Process* p, process::ProcessContext* context)
{
  sendBufferedOutput();

  ExecuteCommandOutputRequest* ecoRequest = new ExecuteCommandOutputRequest();
  ecoRequest->set_original_request_id(originalRequestId);
  ecoRequest->set_is_exit(1);
  ecoRequest->set_exit_code(context->exit_code_);

  Request req;
  req.set_name("");
  req.set_session_id(sessionId);
  req.set_allocated_execute_output(ecoRequest);

  msgSender->sendRequest(req, 0);

  stopped = 1;
}

void CommandExecutor::sendBufferedOutput() {
  if (!bufferedOutput.empty()) {
    if (remoteShowOutput && isStdout) {
      printf("%s", bufferedOutput.c_str());
    }
    if (remoteShowError && !isStdout) {
      fprintf(stderr, "%s", bufferedOutput.c_str());
    }
    if (localShowOutput && isStdout || localShowError && !isStdout) {
      ExecuteCommandOutputRequest* ecoRequest = new ExecuteCommandOutputRequest();
      ecoRequest->set_original_request_id(originalRequestId);
      ecoRequest->set_is_exit(0);
      ecoRequest->set_is_error_output(!isStdout);
      ecoRequest->set_output(bufferedOutput);

      Request req;
      req.set_name("");
      req.set_session_id(sessionId);
      req.set_allocated_execute_output(ecoRequest);

      msgSender->sendRequest(req, 0);
    }
    std::string().swap(bufferedOutput);
    lastSendTime = base::Time::Now().ToInternalValue();
  }
}

void CommandExecutor::OnOutput(process::Process* p, bool is_std_out, const std::string& data)
{
  if (!bufferedOutput.empty()) {
    if ((bool)isStdout != is_std_out) {
      sendBufferedOutput();
    }
  }

  isStdout = (int)is_std_out;
  bufferedOutput += data;
  if (bufferedOutput.size() > 1024 || base::Time::Now().ToInternalValue() - lastSendTime > 50000) {
    sendBufferedOutput();
  }
}

void CommandExecutor::scheduleFlushOutput() {
  if (stopped) {
    return;
  }
  base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&CommandExecutor::flushOutput, weakptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(1000));
}

void CommandExecutor::flushOutput(base::WeakPtr<CommandExecutor> self) {
  if (CommandExecutor* pThis = self.get()) {
    self->flushOutputImpl();
  }
}

void CommandExecutor::flushOutputImpl() {
  sendBufferedOutput();
  scheduleFlushOutput();
}
}