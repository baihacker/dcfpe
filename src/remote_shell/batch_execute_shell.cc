#include "remote_shell/batch_execute_shell.h"

#include "dpe_base/dpe_base.h"

extern std::string get_iface_address();

namespace rs {
BatchExecuteShell::BatchExecuteShell(BatchExecuteShellHost* host):
  host(host),
  nextCommandIndex(0),
  showPrompt(false),
  weakptr_factory_(this) {
}

BatchExecuteShell::~BatchExecuteShell() {
  serverNode = NULL;
}

void BatchExecuteShell::start(const std::string& target, const std::vector<std::string>& cmdLines) {
  this->cmdLines = cmdLines;
  nextCommandIndex = 0;
  serverNode = new rs::LocalServerNode(this, get_iface_address());

  if (!serverNode->start()) {
    outputPrompt("Cannot start local server.\n");
    willStop(false);
  } else {
    outputPrompt("Connecting to " + target + "\n");
    serverNode->connectToTarget(target);
  }
}

void BatchExecuteShell::willStop(bool succeed) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(BatchExecuteShell::stop, weakptr_factory_.GetWeakPtr(), succeed));
}

void BatchExecuteShell::stop(base::WeakPtr<BatchExecuteShell> pThis, bool succeed) {
  if (auto* self = pThis.get()) {
    self->stopImpl(succeed);
  }
}

void BatchExecuteShell::stopImpl(bool succeed) {
  host->onStop(succeed);
}

void BatchExecuteShell::willRunNextCommand() {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(BatchExecuteShell::runNextCommand, weakptr_factory_.GetWeakPtr()));
}

void BatchExecuteShell::runNextCommand(base::WeakPtr<BatchExecuteShell> pThis) {
  if (auto* self = pThis.get()) {
    self->runNextCommandImpl();
  }
}

void BatchExecuteShell::runNextCommandImpl() {
  if (nextCommandIndex == cmdLines.size()) {
    willStop(true);
  } else {
    const std::string& cmd = cmdLines[nextCommandIndex++];
    if (serverNode->executeCommandRemotely(cmd)) {
      // An "exit" or "q" command is executed.
      outputPrompt("\nExecuting " + cmd + "\n");
      if (serverNode->canExitNow()) {
        willStop(true);
      }
    } else {
      outputPrompt("Cannot execute " + cmd + "\n");
      willStop(true);
    }
  }
}

void BatchExecuteShell::onConnectStatusChanged(int newStatus) {
  if (newStatus == ServerStatus::TIMEOUT) {
    outputPrompt("Time out!\n");
    willStop(false);
  } else if (newStatus == ServerStatus::FAILED) {
    outputPrompt("Cannot connect to %s " + serverNode->target() + "!\n");
    willStop(false);
  } else if (newStatus == ServerStatus::CONNECTED) {
    outputPrompt("Connected!\n");
    willRunNextCommand();
  }
}

void BatchExecuteShell::onCommandStatusChanged(int newStatus) {
  if (newStatus == ServerStatus::TIMEOUT) {
    outputPrompt("Command time out!\n");
    willStop(false);
  } else if (newStatus == ServerStatus::FAILED) {
    outputPrompt("Command failed!\n");
    willStop(false);
  } else {
    willRunNextCommand();
  }
}
}
