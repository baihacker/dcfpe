#include "remote_shell/command_executor.h"

#include "dpe_base/dpe_base.h"
#include "remote_shell/proto/rs.pb.h"
#include <iostream>

namespace rs
{


CommandExecutor::CommandExecutor(): weakptr_factory_(this), isStdout(1), lastSendTime(0) {
}

CommandExecutor::~CommandExecutor() {
}

static void deleteExecutorImpl(base::WeakPtr<CommandExecutor> executor) {
  if (auto* who = executor.get()) {
    delete who;
  }
}

static void willDeleteExecutor(CommandExecutor* executor) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(deleteExecutorImpl, executor->getWeakPtr()));
}

std::string CommandExecutor::execute(const ExecuteCommandRequest& command, int64_t originalRequestId) {
  this->originalRequestId = originalRequestId;
  msgSender = new MessageSender(command.address());

  base::FilePath imagePath(base::UTF8ToNative("C:\\Windows\\System32\\cmd.exe"));
  std::string arg = command.cmd();
  for (auto& iter: command.args()) {
    arg = arg + " " + iter;
  }

  process = new process::Process(this);
  auto& option = process->GetProcessOption();
  option.image_path_ = imagePath;
  option.argument_list_r_.push_back(base::UTF8ToNative("/C"));
  option.argument_list_r_.push_back(base::UTF8ToNative(arg));
  option.redirect_std_inout_ = true;


  if (!process->Start()) {
    return "";
  }

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
  req.set_allocated_execute_output(ecoRequest);

  msgSender->sendRequest(req, 0);

  willDeleteExecutor(this);
}

void CommandExecutor::sendBufferedOutput() {
  if (!bufferedOutput.empty()) {
    ExecuteCommandOutputRequest* ecoRequest = new ExecuteCommandOutputRequest();
    ecoRequest->set_original_request_id(originalRequestId);
    ecoRequest->set_is_exit(0);
    ecoRequest->set_is_error_output(!isStdout);
    ecoRequest->set_output(bufferedOutput);

    Request req;
    req.set_name("");
    req.set_allocated_execute_output(ecoRequest);

    msgSender->sendRequest(req, [=](int32_t zmqError, const Response& response) {
      if (zmqError != 0 || response.error_code() != 0) {
        if (this->process.get()) {
          this->process->Stop();
          this->process = NULL;
        }
        willDeleteExecutor(this);
      }
    }, 10*1000);
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
  if (bufferedOutput.size() > 1024 || base::Time::Now().ToInternalValue() - lastSendTime > 100000) {
    sendBufferedOutput();
  }
}
}
